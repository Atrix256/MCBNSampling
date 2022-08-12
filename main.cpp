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

struct Point
{
    int classIndex = -1;
    Vec2 v;
};

#include "PaperDataSets.h"

void DrawDot(unsigned char* pixels, int imageSize, int x, int y, float radius, const unsigned char (&RGB)[3])
{
    int paddedRadius = int(radius) + 3;

    for (int iy = -paddedRadius; iy <= paddedRadius; ++iy)
    {
        int py = (y + iy + imageSize) % imageSize;
        for (int ix = -paddedRadius; ix <= paddedRadius; ++ix)
        {
            int px = (x + ix + imageSize) % imageSize;

            float distanceToSurface = ToroidalDistance(Vec2{ (float)x, (float)y }, Vec2{ (float)px, (float)py }, float(imageSize));
            distanceToSurface -= radius;

            float alpha = 1.0f - SmoothStep(0.0f, 2.0f, distanceToSurface);

            if (alpha > 0)
            {
                unsigned char* pixel = &pixels[(py * imageSize + px) * 3];
                pixel[0] = (unsigned char)Lerp((float)pixel[0], (float)RGB[0], alpha);
                pixel[1] = (unsigned char)Lerp((float)pixel[1], (float)RGB[1], alpha);
                pixel[2] = (unsigned char)Lerp((float)pixel[2], (float)RGB[2], alpha);
            }
        }
    }
}

void MakeSamplesImage(const char* baseFileName, const std::vector<Point>& points, int imageSize = 256, float dotSize = 0.5f)
{
    // show metrics
    float minX = FLT_MAX;
    float minY = FLT_MAX;
    float maxX = -FLT_MAX;
    float maxY = -FLT_MAX;
    std::vector<int> classCounts;
    for (const Point& p : points)
    {
        if (p.classIndex >= classCounts.size())
            classCounts.resize(p.classIndex + 1, 0);
        classCounts[p.classIndex]++;

        minX = std::min(minX, p.v[0]);
        maxX = std::max(maxX, p.v[0]);
        minY = std::min(minY, p.v[1]);
        maxY = std::max(maxY, p.v[1]);
    }
    printf("Making %s\n%i points\n", baseFileName, (int)points.size());
    for (int i = 0; i < classCounts.size(); ++i)
        printf("  %i : %i\n", i, classCounts[i]);
    //printf("min/max = (%f, %f) - (%f, %f)\n", minX, minY, maxX, maxY);

    // make images
    {
        std::vector<std::vector<unsigned char>> images(classCounts.size() + 1);
        for (auto& pixels : images)
            pixels.resize(imageSize * imageSize * 3, 255);

        for (const Point& p : points)
        {
            Vec3 RGBf = IndexToColor(p.classIndex, 1.0f, 0.95f);
            unsigned char RGBU8[3] = {
                (unsigned char)Clamp(RGBf[0] * 256.0f, 0.0f, 255.0f),
                (unsigned char)Clamp(RGBf[1] * 256.0f, 0.0f, 255.0f),
                (unsigned char)Clamp(RGBf[2] * 256.0f, 0.0f, 255.0f)
            };

            int x = (int)Clamp(p.v[0] * float(imageSize), 0.0f, float(imageSize - 1));
            int y = (int)Clamp(p.v[1] * float(imageSize), 0.0f, float(imageSize - 1));

            DrawDot(images[0].data(), imageSize, x, y, dotSize, RGBU8);
            DrawDot(images[p.classIndex + 1].data(), imageSize, x, y, dotSize, RGBU8);
        }

        for (int i = 0; i < classCounts.size() + 1; ++i)
        {
            char fileName[1024];
            if (i == 0)
                sprintf(fileName, "%s.png", baseFileName);
            else
                sprintf(fileName, "%s.%i.png", baseFileName, i - 1);
            stbi_write_png(fileName, imageSize, imageSize, 3, images[i].data(), 0);
        }
    }
}


static const int c_failCountFatal = 1000;
static const int c_failCountRemove = 100;

struct HardLayer
{
    float radius = 0.0f;
};

template <size_t N>
std::vector<Point> MCBNHardDisk(const HardLayer(&layers_)[N], int targetCount)
{
    // TODO: make sure all the variable names are good etc

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
    std::vector<Point> ret;
    {
        int pointsRemoved = 0;
        pcg32_random_t rng = GetRNG();

        int failCount = 0;
        while(ret.size() < targetCount && pointsRemoved < targetCount)
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
            std::vector<int> conflicts;
            float newClassPercent = float(layersInternal[leastPercentClass].sampleCount) / float(layersInternal[leastPercentClass].targetCount);
            bool considerRemoval = ((failCount + 1) % c_failCountRemove) == 0;
            for (int pointIndex = 0; pointIndex < ret.size(); ++pointIndex)
            {
                // TODO: switch to toroidal distance after things are working? Note that in the notes too
                const Vec2& v = ret[pointIndex].v;
                int classIndex = ret[pointIndex].classIndex;
                float distance = Distance(v, point);
                if (distance < rMatrix[classIndex][leastPercentClass])
                {
                    satisfies = false;

                    considerRemoval = considerRemoval &&
                        (float(layersInternal[classIndex].sampleCount) / float(layersInternal[classIndex].targetCount) >= newClassPercent) &&
                        (layers[classIndex].radius >= layers[leastPercentClass].radius);

                    if (considerRemoval)
                        conflicts.push_back(pointIndex);
                }
            }

            if (satisfies)
            {
                failCount = 0;
                ret.push_back({ leastPercentClass, point });
                layersInternal[leastPercentClass].sampleCount++;
            }
            else
            {
                failCount++;

                if (considerRemoval)
                {
                    std::sort(conflicts.begin(), conflicts.end(), [](int a, int b) { return b < a; });

                    // TODO: figure out how they map k to point count and take that code.
                    // TODO: check out their params for how often to check for removal, and when to give up and exit.

                    float layerPercent = float(layersInternal[leastPercentClass].sampleCount) / float(layersInternal[leastPercentClass].targetCount);
                    for (int pointIndex : conflicts)
                    {
                        layersInternal[ret[pointIndex].classIndex].sampleCount--;
                        ret.erase(ret.begin() + pointIndex);

                        pointsRemoved++;
                    }
                }
                else if (failCount > c_failCountFatal)
                    break;
            }
        }
    }

    // unsort the layers, so they are in the same order that the user asked for
    for (int i = 0; i < N; ++i)
    {
        for (int j = i; j < N; ++j)
        {
            if (layers[j].originalIndex == i)
            {
                if (i != j)
                {
                    std::swap(layers[i], layers[j]);
                    std::swap(layersInternal[i], layersInternal[j]);
                    for (Point& p : ret)
                    {
                        if (p.classIndex == i)
                            p.classIndex = j;
                        else if (p.classIndex == j)
                            p.classIndex = 1;
                    }
                }
                break;
            }
        }
    }

    return ret;
}

int main(int argc, char** argv)
{
    _mkdir("out");

    MakeSamplesImage("out/hard", MCBNHardDisk({ {0.04f}, {0.02f}, {0.01f} }, 10000));
    MakeSamplesImage("out/MCBNSPaper", GetPaperDataSet());

    // ALSO: how do they calculate the target point count?
    //MakeSamplesImage("out/hard.png", MCBNHardDisk({ {3.0f}, {2.0f}, {1.0f} }, 1000));

    return 0;
}
/*
TODO:
- hard disk isn't working right yet. why not?
- hard disk with adaptive sampling
- soft disk implementation
- same vs different weights
- DFT of output images

Notes:
- not a fan of dart throwing blue noise (show why via DFT?)
- Theirs is faster than mine cause they use a grid for acceleration
- The "remove" logic is better explained as "can i put this point here anyways and remove everything in conflict?"
- they don't use toroidal distance, just regular distance. toroidal tiles better.

TODO: could revive other repo by forking it.
command line param: .\DartThrowing.exe 2 3 1 1 1 0.04 0.02 0.01 4 1 1 > out.txt
*/

