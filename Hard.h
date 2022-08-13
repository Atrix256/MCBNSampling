#pragma once

#include "PaperDataSetsHard.h"
#include "Grid.h"

namespace Hard
{
    struct Layer
    {
        float radius = 0.0f;
        int originalIndex = 0;
        int sampleCount = 0;
        int targetCount = 0;
    };

    template <size_t N, typename RNG>
    std::vector<Point> Make(const float(&radii)[N], int targetCount, RNG& rng)
    {
        const int c_failCountFatal = targetCount * 10;
        const int c_failCountRemove = targetCount / 10;

        std::vector<Grid<100,100>> grids(N);

        // sort the layers from largest to smallest radius
        std::vector<Layer> layers(N);
        for (int i = 0; i < N; ++i)
        {
            layers[i].radius = radii[i];
            layers[i].originalIndex = i;
        }

        std::sort(
            layers.begin(),
            layers.end(),
            [](const Layer& A, const Layer& B)
            {
                return A.radius > B.radius;
            }
        );

        // Calculate the targetCount for each layer.
        {
            float sumInverseRadiusSquared = 0.0f;
            for (const Layer& layer : layers)
                sumInverseRadiusSquared += 1.0f / (layer.radius * layer.radius);
            for (int i = 0; i < N; ++i)
            {
                float percent = (1.0f / (layers[i].radius * layers[i].radius)) / sumInverseRadiusSquared;
                layers[i].targetCount = int(float(targetCount) * percent);
            }
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
                Vec2 point = rng();
                std::vector<int> conflicts;
                float newClassPercent = float(layers[leastPercentClass].sampleCount) / float(layers[leastPercentClass].targetCount);
                bool considerRemoval = ((failCount + 1) % c_failCountRemove) == 0;

                // find conflicting points using the grids
                for (int i = 0; i < N; ++i)
                    grids[i].GetPoints(point[0], point[1], rMatrix[i][leastPercentClass], conflicts);

                if (conflicts.size() == 0)
                {
                    failCount = 0;
                    ret.push_back({ leastPercentClass, point });
                    layers[leastPercentClass].sampleCount++;
                    grids[leastPercentClass].AddPoint((int)ret.size() - 1, point[0], point[1]);
                }
                else
                {
                    failCount++;

                    if (considerRemoval)
                    {
                        // see if it's safe to remove all of the points or not
                        for (int pointIndex : conflicts)
                        {
                            int classIndex = ret[pointIndex].classIndex;
                            considerRemoval = considerRemoval &&
                                (float(layers[classIndex].sampleCount) / float(layers[classIndex].targetCount) >= newClassPercent) &&
                                (layers[classIndex].radius >= layers[leastPercentClass].radius);
                            if (!considerRemoval)
                                break;
                        }

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

                                // All grids need to be updated to know this point was removed.
                                // Any index > this is invalid unless it is decrimented
                                for (int classIndex = 0; classIndex < N; ++classIndex)
                                    grids[classIndex].RemovePoint(pointIndex);

                                pointsRemoved++;
                            }
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
