#pragma once

#include "Grid.h"

namespace HardAdaptive
{
    struct LayerParam
    {
        const char* imageFileName = nullptr;
        float rmin = 0.0f;
        float rmax = 0.0f;
    };

    struct Layer : LayerParam
    {
        std::vector<float> imageRadius;
        float imageExpectedRadius = 0.0f;

        int originalIndex = 0;
        int sampleCount = 0;
        int targetCount = 0;
    };

    void LoadImage(Layer& layer, int targetW, int targetH)
    {
        int imageW, imageH, imageComp;
        stbi_uc* pixelsu8 = stbi_load(layer.imageFileName, &imageW, &imageH, &imageComp, 1);

        layer.imageRadius.resize(targetW * targetH);
        for (int i = 0; i < (int)layer.imageRadius.size(); ++i)
        {
            // get the destination pixel coordinates
            int destx = i % targetW;
            int desty = i / targetW;

            // convert to uv
            float u = float(destx) / float(targetW - 1);
            float v = float(desty) / float(targetH - 1);

            // get as source pixel coordinates
            float srcxf = u * float(imageW);
            float srcyf = v * float(imageH);
            int srcx = int(srcxf);
            int srcy = int(srcyf);
            float xfract = srcxf - std::floor(srcxf);
            float yfract = srcyf - std::floor(srcyf);

            // bilinear interpolate source image
            float v00 = float(pixelsu8[(srcy + 0) * imageW + (srcx + 0)]) / 255.0f;
            float v01 = float(pixelsu8[(srcy + 0) * imageW + (srcx + 1)]) / 255.0f;
            float v10 = float(pixelsu8[(srcy + 1) * imageW + (srcx + 0)]) / 255.0f;
            float v11 = float(pixelsu8[(srcy + 1) * imageW + (srcx + 1)]) / 255.0f;

            float v0x = Lerp(v00, v01, xfract);
            float v1x = Lerp(v10, v11, xfract);
            float vyx = Lerp(v0x, v1x, yfract);

            // store the value
            layer.imageRadius[i] = Lerp(layer.rmin, layer.rmax, vyx);
            layer.imageExpectedRadius = Lerp(layer.imageExpectedRadius, layer.imageRadius[i], 1.0f / float(i+1));
        }

        stbi_image_free(pixelsu8);
    }

    template <size_t N, typename RNG>
    std::vector<Point> Make(const LayerParam(&layers_)[N], int imageW, int imageH, int targetCount, RNG& rng)
    {
        const int c_failCountFatal = targetCount * 20;
        const int c_failCountRemove = targetCount / 10;

        // sort the layers from largest to smallest radius
        std::vector<Layer> layers(N);
        for (int i = 0; i < N; ++i)
        {
            layers[i].imageFileName = layers_[i].imageFileName;
            layers[i].rmin = layers_[i].rmin;
            layers[i].rmax = layers_[i].rmax;
            layers[i].originalIndex = i;
            LoadImage(layers[i], imageW, imageH);
            //printf("[%i] %f\n", i, layers[i].imageExpectedRadius);
        }

        std::sort(
            layers.begin(),
            layers.end(),
            [](const Layer& A, const Layer& B)
            {
                return A.imageExpectedRadius > B.imageExpectedRadius;
            }
        );

        // Calculate the targetCount for each layer.
        {
            float sumInverseRadiusSquared = 0.0f;
            for (const Layer& layer : layers)
                sumInverseRadiusSquared += 1.0f / (layer.imageExpectedRadius * layer.imageExpectedRadius);
            for (int i = 0; i < N; ++i)
            {
                float percent = (1.0f / (layers[i].imageExpectedRadius * layers[i].imageExpectedRadius)) / sumInverseRadiusSquared;
                layers[i].targetCount = int(float(targetCount) * percent);
                //printf("[%i] %i target, %0.2f percent\n", i, layers[i].targetCount, 100.0f * percent);
            }
        }

        // Make the r matrix
        typedef std::array<std::array<float, N>, N> TrMatrix;
        std::vector<TrMatrix> rMatrices(imageW * imageH);
        for (int i = 0; i < (int)rMatrices.size(); ++i)
        {
            TrMatrix& rMatrix = rMatrices[i];

            int ix = i % imageW;
            int iy = i / imageW;

            std::vector<float> layerPixelRadius(N, 0.0f);

            for (int i = 0; i < N; ++i)
            {
                std::fill(rMatrix[i].begin(), rMatrix[i].end(), 0.0f);
                layerPixelRadius[i] = layers[i].imageRadius[iy * imageW + ix];
                rMatrix[i][i] = layerPixelRadius[i];
            }

            int classStartIndex = -1;
            int classEndIndex = 0;
            float totalDensity = 0.0f;
            while (true)
            {
                classStartIndex = classEndIndex;
                if (classStartIndex >= N)
                    break;

                classEndIndex++;

                for (int i = classStartIndex; i < classEndIndex; ++i)
                    totalDensity += 1.0f / (layerPixelRadius[i] * layerPixelRadius[i]);

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
            int lastPercent = -1;
            int failCount = 0;
            while (ret.size() < targetCount && pointsRemoved < targetCount)
            {
                int percent = int(100.0f * std::max(float(ret.size()) / float(targetCount), float(pointsRemoved) / float(targetCount)));
                if (percent != lastPercent)
                {
                    printf("\r%i%%", percent);
                    lastPercent = percent;
                }

                // find the class which is least filled.
                float leastPercent = FLT_MAX;
                int leastPercentClass = -1;
                for (int i = 0; i < N; ++i)
                {
                    float percent = float(layers[i].sampleCount) / float(layers[i].targetCount);
                    if (percent < leastPercent)
                    {
                        leastPercent = percent;
                        leastPercentClass = i;
                    }
                }

                // Calculate a random point and accept it if it satisfies all constraints
                // Every so often, take it anyways, and destroy the conflicting points (with some more logic)
                Vec2u pointu = rng(imageW - 1, imageH - 1);
                Vec2 point = Vec2
                {
                    float(pointu[0]) / float(imageW - 1),
                    float(pointu[1]) / float(imageH - 1)
                };
                std::vector<int> conflicts;
                float newClassPercent = float(layers[leastPercentClass].sampleCount) / float(layers[leastPercentClass].targetCount);
                bool considerRemoval = ((failCount + 1) % c_failCountRemove) == 0;

                // TODO: could probably speed this up with a grid, starting at the cell and increasing radius until the whole
                // grid has been queried. This would find more relevant points sooner in the long run.

                // find conflicting points
                // If we are considering removal, we want all conflicts
                // otherwise we only need 1 point to know that there was a conflict
                TrMatrix& candidateRMatrix = rMatrices[pointu[1] * imageW + pointu[0]];
                for (size_t pointIndex = 0; pointIndex < ret.size(); ++pointIndex)
                {
                    const Point& existingPoint = ret[pointIndex];
                    Vec2u existingPointU = Vec2u
                    {
                        (uint32_t)Clamp(existingPoint.v[0] * float(imageW), 0.0f, float(imageW - 1)),
                        (uint32_t)Clamp(existingPoint.v[1] * float(imageH), 0.0f, float(imageH - 1))
                    };
                    TrMatrix& existingRMatrix = rMatrices[existingPointU[1] * imageW + existingPointU[0]];
                    float minDistance =
                        (candidateRMatrix[leastPercentClass][existingPoint.classIndex] +
                        existingRMatrix[leastPercentClass][existingPoint.classIndex]) / 2.0f;

                    if (ToroidalDistanceSq(existingPoint.v, point) < minDistance)
                    {
                        conflicts.push_back((int)pointIndex);

                        // If we are considering removal, cancel it if this point is higher priority
                        considerRemoval = considerRemoval &&
                            float(layers[existingPoint.classIndex].sampleCount) / float(layers[existingPoint.classIndex].targetCount) >= newClassPercent &&
                            1.0f / existingRMatrix[leastPercentClass][existingPoint.classIndex] >= 1.0f / candidateRMatrix[leastPercentClass][existingPoint.classIndex];

                        // If we aren't considering removal, we only need one conflict to keep going
                        if (!considerRemoval)
                            break;
                    }
                }

                if (conflicts.size() == 0)
                {
                    failCount = 0;
                    ret.push_back({ leastPercentClass, point });
                    layers[leastPercentClass].sampleCount++;
                }
                else
                {
                    failCount++;

                    if (considerRemoval)
                    {
                        // sort highest to lowest so we don't invalidate the indices we are removing
                        std::sort(conflicts.begin(), conflicts.end(), [](int a, int b) { return b < a; });

                        float layerPercent = float(layers[leastPercentClass].sampleCount) / float(layers[leastPercentClass].targetCount);
                        for (int pointIndex : conflicts)
                        {
                            if (pointIndex < 0 || pointIndex >= ret.size())
                                printf("ERROR! pointIndex = %i.  ret.size() = %i\n", pointIndex, (int)ret.size());
                            layers[ret[pointIndex].classIndex].sampleCount--;
                            ret.erase(ret.begin() + pointIndex);
                            pointsRemoved++;
                        }
                    }
                    else if (failCount > c_failCountFatal)
                        break;
                }
            }
        }
        printf("\r100%%\n");

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
};
