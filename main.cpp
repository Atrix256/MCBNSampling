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

#include "Hard.h"
#include "Soft.h"

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

    // Make text files
    {
        // text file
        {
            char fileName[1024];
            sprintf(fileName, "%s.txt", baseFileName);
            FILE* file = nullptr;
            fopen_s(&file, fileName, "wb");
            for (const Point& p : points)
                fprintf(file, "%i %f %f\n", p.classIndex, p.v[0], p.v[1]);
            fclose(file);
        }

        // csv file
        {
            char fileName[1024];
            sprintf(fileName, "%s.csv", baseFileName);
            FILE* file = nullptr;
            fopen_s(&file, fileName, "wb");
            fprintf(file, "\"Class\",\"x\",\"y\"\n");
            for (const Point& p : points)
                fprintf(file, "\"%i\",\"%f\",\"%f\"\n", p.classIndex, p.v[0], p.v[1]);
            fclose(file);
        }

        // .h file
        {
            char fileName[1024];
            sprintf(fileName, "%s.h", baseFileName);
            FILE* file = nullptr;
            fopen_s(&file, fileName, "wb");
            fprintf(file,
                "struct MultiClassPoint\n"
                "{\n"
                "    int classIndex;\n"
                "    float x;\n"
                "    float y;\n"
                "};\n"
                "\n"
                "MultiClassPoint points[] =\n"
                "{\n"
            );
            for (const Point& p : points)
                fprintf(file, "    { %i, %ff, %ff },\n", p.classIndex, p.v[0], p.v[1]);
            fprintf(file, "};\n");
            fclose(file);
        }
    }

    // make images
    {
        int imageCount = (1 << classCounts.size()) - 1;

        std::vector<std::vector<unsigned char>> images(imageCount);
        std::vector<std::vector<unsigned char>> imagesbw(imageCount);
        for (auto& pixels : images)
            pixels.resize(imageSize * imageSize * 3, 255);
        for (auto& pixels : imagesbw)
            pixels.resize(imageSize * imageSize, 255);

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

            for (int i = 0; i < imageCount; ++i)
            {
                if ((i + 1) & (1 << p.classIndex))
                {
                    DrawDot(images[i].data(), imageSize, x, y, dotSize, RGBU8);
                    imagesbw[i][y * imageSize + x] = 0;
                }
            }
        }

        for (int i = 0; i < imageCount; ++i)
        {
            std::vector<char> mask(classCounts.size()+1, 0);
            for (int j = 0; j < classCounts.size(); ++j)
                mask[classCounts.size() - j - 1] = (((i + 1) & (1 << j)) != 0) ? '1' : '0';

            char fileName[1024];

            sprintf(fileName, "%s_color.%s.png", baseFileName, mask.data());
            stbi_write_png(fileName, imageSize, imageSize, 3, images[i].data(), 0);

            sprintf(fileName, "%s_bw.%s.png", baseFileName, mask.data());
            stbi_write_png(fileName, imageSize, imageSize, 1, imagesbw[i].data(), 0);
        }

        // make a tiled image
        {
            char fileName[1024];

            std::vector<unsigned char> tiled(imageSize * imageSize * 3 * 9, 255);
            unsigned char* dest = tiled.data();

            for (int i = 0; i < imageSize * 3; ++i)
            {
                const unsigned char* src = &images[images.size() - 1][(i % imageSize) * imageSize * 3];

                memcpy(dest, src, imageSize * 3);
                dest += imageSize * 3;

                memcpy(dest, src, imageSize * 3);
                dest += imageSize * 3;

                memcpy(dest, src, imageSize * 3);
                dest += imageSize * 3;
            }

            sprintf(fileName, "%s_color.tiled.png", baseFileName);
            stbi_write_png(fileName, imageSize * 3, imageSize * 3, 3, tiled.data(), 0);
        }
    }
}

Vec2 RNGContinuous()
{
    static pcg32_random_t rng = GetRNG();
    return Vec2
    {
        RandomFloat01(rng),
        RandomFloat01(rng)
    };
}

template <size_t X, size_t Y>
Vec2 RNGDiscrete()
{
    static pcg32_random_t rng = GetRNG();
    Vec2 ret = Vec2
    {
        RandomFloat01(rng),
        RandomFloat01(rng)
    };

    ret[0] = std::floor(ret[0] * float(X - 1) + 0.5f) / float(X); // TODO: verify this is correct when you are less tired.
    ret[1] = std::floor(ret[1] * float(Y - 1) + 0.5f) / float(Y);

    return ret;
}

std::vector<Point> GetPointsFromTextFile(const char* fileName)
{
    FILE* file = nullptr;
    fopen_s(&file, fileName, "rt");

    std::vector<Point> ret;
    while (true)
    {
        int a;
        float b, c;
        if (fscanf(file, "%i %f %f", &a, &b, &c) != 3)
            break;

        ret.push_back({ a, {b, c} });
    }

    fclose(file);

    return ret;
}

int main(int argc, char** argv)
{
    _mkdir("out");

    MakeSamplesImage("out/softCT", Soft::Make({ 100, 1000, 4000 }, RNGContinuous, true));
    MakeSamplesImage("out/softCF", Soft::Make({ 100, 1000, 4000 }, RNGContinuous, false));

    MakeSamplesImage("out/soft10x10", Soft::Make({ 5, 10, 20 }, RNGDiscrete<10, 10>, true));
    MakeSamplesImage("out/soft100x100", Soft::Make({ 100, 1000, 4000 }, RNGDiscrete<100,100>, true));
    MakeSamplesImage("out/soft256x256", Soft::Make({ 100, 1000, 4000 }, RNGDiscrete<256, 256>, true));

    for (int i = 0; i < 10; ++i)
    {
        char fileName[1024];
        sprintf(fileName, "out/hardT%i", i);
        MakeSamplesImage(fileName, Hard::Make({ {0.04f}, {0.02f}, {0.01f} }, 10000, RNGContinuous, true));
    }
    MakeSamplesImage("out/hardF", Hard::Make({ {0.04f}, {0.02f}, {0.01f} }, 10000, RNGContinuous, false));

    for (int i = 0; i < 10; ++i)
    {
        char fileNameSrc[1024];
        char fileNameDest[1024];
        sprintf(fileNameSrc, "paperdata/Hard%i.txt", i);
        sprintf(fileNameDest, "out/MCBNSPaperHard%i", i);
        MakeSamplesImage(fileNameDest, GetPointsFromTextFile(fileNameSrc));
    }

    return 0;
}
/*
TODO:
- DFT of pure black/white output images. need them all to be in the same range. maybe give a file pattern like *bw.*.png?
- could do the density map feature. might help w/ your own code.
 - probably copy hard.h into a hard adaptive.h or something
- maybe wait to put this out until your paper so you don't get scooped? (ha! but ... shrug)
- after this, wasn't there another paper you needed to look at?
 - properties of jointly blue noise masks...
! oddly, the other paper makes noise that looks good tiled. how? it tiles better than your toroidal noise. maybe 3 sigmas isn't enough? i dunno.

Paper TODO:s
- average the DFT of 10 of results from paper, and of your results? to compare quality
 - they aren't doing removal code, and they aren't doing toroidal distance.
 - should also compare their adaptive sampling vs yours. visual quality test. or maybe do N of them and average?
  - which kinda leads into your spatiotemporal results actually...
- could make this spatiotemporal too probably... just add the energy function on z axis

Notes:
- not a fan of dart throwing blue noise (show why via DFT?)
- Theirs is faster than mine cause they use a grid for acceleration
- The "remove" logic is better explained as "can i put this point here anyways and remove everything in conflict?"
- they don't use toroidal distance, just regular distance. toroidal tiles better.
- NGL, the other code isn't easy to read and i saw weird things like an empty priority group added to the list, and the number of priority groups affecting sample calculations.
 - so, some minor bugs seemingly :shrug:
 - hard to know what parameters to give, and the readme that gives an example command line argument is out of date
 - the params it specifies don't cause removal to happen.

NOTE:num trial calculation: (hard disk)
amplification = sqrt(2) / minimum r matrix value
cellsize = 1 / amplification
total trials = k_number * amplification * amplification
but fill and such are totally based on trial count. fill doesn't have to be. it could be percent of total, and compare that vs r matrix or something?

TODO: could revive other repo by forking it.
command line param: .\DartThrowing.exe 2 3 1 1 1 0.04 0.02 0.01 4 1 1 > out.txt
- put notes above. esp, that they aren't removing and they aren't doing toroidal distance which affects DFT.
*/

