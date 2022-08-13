#pragma once

namespace Soft
{
    struct Layer
    {
        float radius = 0.0f;
        int originalIndex = 0;
        int sampleCount = 0;
        int targetCount = 0;
    };

    template <size_t N>
    std::vector<Point> Make(const int(&counts)[N])
    {
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
            pcg32_random_t rng = GetRNG();

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

                // TODO: is this the best strategy here? a k of 1 i mean
                int candidateCount = int(ret.size()) + 1;
                for (int i = 0; i < candidateCount; ++i)
                {
                    Vec2 candidate = Vec2{ RandomFloat01(rng), RandomFloat01(rng) };

                    // TODO: accel this with grids and a max search radius based on sigma
                    float score = 0.0f;
                    for (const Point& p : ret)
                    {
                        float r = rMatrix[leastPercentClass][p.classIndex];
                        float sigma = 0.25f * r;
                        float tdsq = ToroidalDistanceSq(p.v, candidate);
                        score += exp(-(tdsq) / (2.0f * sigma * sigma));
                    }

                    if (score < bestScore)
                    {
                        bestCandidate = candidate;
                        bestScore = score;
                    }
                }

                ret.push_back({ leastPercentClass, {bestCandidate} });
                layers[leastPercentClass].sampleCount++;
            }

            // TODO: continue and finish this. mitchell's best candidate with an energy field
            // energy field has sigma of 1/4 of the r matrix value.
            // how many candidates? what was the sigma again? we could accel this with a grid if we have a max distance that energy can travel
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
