#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <vector>
#include <array>
#include <direct.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "Random.h"
#include "MathUtils.h"
#include "IndexToColor.h"

typedef std::array<float, 2> Vec2;
typedef std::array<float, 3> Vec3;

template <size_t N>
void MakeSamplesImage(const char* fileName, const std::array<std::vector<Vec2>, N>& points, int imageSize = 128)
{
    std::vector<unsigned char> pixels(imageSize * imageSize * 3, 255);

    for (int i = 0; i < N; ++i)
    {
        Vec3 RGBf = IndexToColor(i, 1.0f, 0.95f);
        unsigned char RGBU8[3] = {
            (unsigned char)Clamp(RGBf[0] * 256.0f, 0.0f, 255.0f),
            (unsigned char)Clamp(RGBf[1] * 256.0f, 0.0f, 255.0f),
            (unsigned char)Clamp(RGBf[2] * 256.0f, 0.0f, 255.0f)
        };

        for (const Vec2& v : points[i])
        {
            int x = (int)Clamp(v[0] * float(imageSize), 0.0f, float(imageSize - 1));
            int y = (int)Clamp(v[1] * float(imageSize), 0.0f, float(imageSize - 1));

            pixels[(y * imageSize + x) * 3 + 0] = RGBU8[0];
            pixels[(y * imageSize + x) * 3 + 1] = RGBU8[1];
            pixels[(y * imageSize + x) * 3 + 2] = RGBU8[2];
        }
    }

    stbi_write_png(fileName, imageSize, imageSize, 3, pixels.data(), 0);
}


static const int c_failCountFatal = 1000;
static const int c_failCountRemove = 100;

struct HardLayer
{
    float radius = 0.0f;
};

template <size_t N>
std::array<std::vector<Vec2>, N> MCBNHardDisk(const HardLayer(&layers_)[N], int targetCount)
{
    struct HardLayerInternal : public HardLayer
    {
        int originalIndex = 0;
    };

    // sort the layers from largest to smallest radius
    std::vector<HardLayerInternal> layers(N);
    for (int i = 0; i < N; ++i)
    {
        layers[i].radius = layers_[i].radius;
        layers[i].originalIndex = i;
    }
    std::sort(layers.begin(), layers.end(), [](const HardLayerInternal& A, const HardLayerInternal& B) { return A.radius > B.radius; });

    struct LayerInternal
    {
        int sampleCount = 0;
        int targetCount = 0;
    };
    std::vector<LayerInternal> layersInternal(N);

    // Calculate the targetCount for each layer.
    float sumInverseRadius = 0.0f;
    for (const HardLayer& layer : layers)
        sumInverseRadius += 1.0f / (layer.radius * layer.radius);
    for (int i = 0; i < N; ++i)
    {
        float percent = (1.0f / (layers[i].radius * layers[i].radius)) / sumInverseRadius;
        layersInternal[i].targetCount = int(float(targetCount) * percent);
    }

    // Make the r matrix
    std::array<std::array<float, N>, N> rMatrix;
    {
        for (int i = 0; i < N; ++i)
        {
            std::fill(rMatrix[i].begin(), rMatrix[i].end(), 0.0f);
            rMatrix[i][i] = layers[i].radius;
        }

        int classStartIndex = -1;
        int classEndIndex = 0;
        float totalDensity = 0.0f;
        while (true)
        {
            classStartIndex = classEndIndex;
            if (classStartIndex >= N)
                break;

            while (classEndIndex < N && layers[classEndIndex].radius == layers[classStartIndex].radius)
                classEndIndex++;

            for (int i = classStartIndex; i < classEndIndex; ++i)
                totalDensity += 1.0f / (layers[i].radius * layers[i].radius);

            for (int i = classStartIndex; i < classEndIndex; ++i)
            {
                for (int j = 0; j < classStartIndex; ++j)
                    rMatrix[i][j] = rMatrix[j][i] = 1.0f / std::sqrt(totalDensity);
            }
        }
    }

    // Make the points!
    std::array<std::vector<Vec2>, N> ret;
    {
        int pointsRemoved = 0;
        int pointsAccepted = 0;
        pcg32_random_t rng = GetRNG();

        int failCount = 0;
        while(pointsAccepted < targetCount && pointsRemoved < targetCount)
        {
            // find the class which is least filled.
            float leastPercent = FLT_MAX;
            int leastPercentClass = -1;
            for (int i = 0; i < N; ++i)
            {
                float percent = float(layersInternal[i].sampleCount) / float(layersInternal[i].targetCount);
                if (percent < leastPercent)
                {
                    leastPercent = percent;
                    leastPercentClass = i;
                }
            }

            // Calculate a random point and accept it if it satisfies all constraints
            Vec2 point = Vec2{ RandomFloat01(rng), RandomFloat01(rng) };
            bool satisfies = true;
            int closestDistanceIndex[N];
            for (int i = 0; i < N; ++i)
            {
                closestDistanceIndex[i] = -1;
                float closestDistance = FLT_MAX;
                for (int j = 0; j < ret[i].size(); ++j)
                {
                    const Vec2& v = ret[i][j];
                    float distance = ToroidalDistance(v, point);
                    if (distance < closestDistance)
                    {
                        closestDistance = distance;
                        closestDistanceIndex[i] = j;
                    }
                }

                if (closestDistance < rMatrix[i][leastPercentClass])
                {
                    satisfies = false;
                }
            }
            if (satisfies)
            {
                failCount = 0;
                ret[leastPercentClass].push_back(point);
                layersInternal[leastPercentClass].sampleCount++;
                pointsAccepted++;
            }
            else
            {
                failCount++;

                if ((failCount % c_failCountRemove) == 0)
                {
                    float layerPercent = float(layersInternal[leastPercentClass].sampleCount) / float(layersInternal[leastPercentClass].targetCount);
                    for (int i = 0; i < N; ++i)
                    {
                        if (closestDistanceIndex[i] == -1)
                            continue;

                        if (layers[i].radius < layers[leastPercentClass].radius)
                            continue;

                        float thisLayerPercent = float(layersInternal[i].sampleCount) / float(layersInternal[i].targetCount);
                        if (thisLayerPercent < layerPercent)
                            continue;

                        ret[i].erase(ret[i].begin() + closestDistanceIndex[i]);
                        pointsAccepted--;
                        layersInternal[i].sampleCount--;
                        pointsRemoved++;
                    }
                }
                else if (failCount > c_failCountFatal)
                    break;
            }
        }
    }

    // unsort the layers, so they are in the same order that the user asked for
    printf("Layers:\n");
    for (int i = 0; i < N; ++i)
    {
        for (int j = i; j < N; ++j)
        {
            if (layers[j].originalIndex == i)
            {
                if (i != j)
                {
                    std::swap(layers[i], layers[j]);
                    std::swap(ret[i], ret[j]);
                }
                break;
            }
        }

        printf("  %i: %i points\n", i, layersInternal[i].sampleCount);
    }

    return ret;
}

int main(int argc, char** argv)
{
    _mkdir("out");

    MakeSamplesImage("out/hard.png", MCBNHardDisk({ {0.2f}, {0.2f}, {0.2f} }, 1000));

    return 0;
}
/*
TODO:
- hard disk isn't working right yet. why not?
- hard disk with adaptive sampling
- soft disk implementation
- same vs different weights

Notes:
- not a fan of dart throwing blue noise (show why via DFT?)

*/

