#pragma once

#include "Grid.h"

namespace Soft
{
    struct Layer
    {
        float radius = 0.0f;
        int originalIndex = 0;
        int sampleCount = 0;
        int targetCount = 0;
    };

    template <size_t N, typename RNG>
    std::vector<Point> Make(const int(&counts)[N], RNG& rng, bool toroidal, int candidateMultiplier = 5)
    {
        std::vector<Grid<100, 100>> grids(N);

        // make the layer data
        int totalCount = 0;
        std::vector<Layer> layers(N);
        for (int i = 0; i < N; ++i)
        {
            float packing_density = c_pi * std::sqrt(3.0f) / 6.0f;
            layers[i].radius = 2.0f * std::pow(packing_density / (c_pi * float(counts[i])), 1.0f / 2.0f);
            layers[i].originalIndex = i;
            layers[i].targetCount = counts[i];
            totalCount += counts[i];
        }

        // sort the layers from largest to smallest radius
        std::sort(
            layers.begin(),
            layers.end(),
            [](const Layer& A, const Layer& B)
            {
                return A.radius > B.radius;
            }
        );

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
            std::vector<float> distances; // out here to avoid allocs
            int lastPercent = -1;
            for (int pointIndex = 0; pointIndex < totalCount; ++pointIndex)
            {
                int percent = int(100.0f * float(pointIndex) / float(totalCount));
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

                Vec2 bestCandidate;
                float bestScore = FLT_MAX;

                int candidateCount = int(ret.size()) * candidateMultiplier + 1;
                for (int i = 0; i < candidateCount; ++i)
                {
                    Vec2 candidate = rng();

                    // Get points within 3 sigmas
                    float score = 0.0f;
                    for (int classIndex = 0; classIndex < N; ++classIndex)
                    {
                        float sigma = 0.25f * rMatrix[leastPercentClass][classIndex];
                        if (toroidal)
                            grids[classIndex].GetPointDistancesSq<true>(candidate[0], candidate[1], 3.0f * sigma, distances, false);
                        else
                            grids[classIndex].GetPointDistancesSq<false>(candidate[0], candidate[1], 3.0f * sigma, distances, false);
                        for (float distSq : distances)
                            score += exp(-(distSq) / (2.0f * sigma * sigma));
                    }

                    // if this score is the best we've seen so far, take it as the new best
                    if (score < bestScore)
                    {
                        bestCandidate = candidate;
                        bestScore = score;
                    }
                }

                // add the point
                ret.push_back({ leastPercentClass, {bestCandidate} });
                layers[leastPercentClass].sampleCount++;
                grids[leastPercentClass].AddPoint((int)ret.size() - 1, bestCandidate[0], bestCandidate[1]);
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
