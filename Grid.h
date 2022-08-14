#pragma once

#include <vector>

template <size_t CELLSX, size_t CELLSY>
class Grid
{
public:
    Grid()
    {
        m_cells.resize(CELLSX);
        for (auto& col : m_cells)
            col.resize(CELLSY);
    }

    static const int XToCellX(float x)
    {
        return int(std::floor(x * float(CELLSX)));
    }

    static const int YToCellY(float y)
    {
        return int(std::floor(y * float(CELLSY)));
    }

    template <bool TOROIDAL>
    void GetPoints(float x, float y, float radius, std::vector<int>& results, bool stopAfterFirst, bool append = true) const
    {
        if (!append)
            results.clear();

        int mincx = XToCellX(x - radius);
        int maxcx = XToCellX(x + radius);
        int mincy = YToCellY(y - radius);
        int maxcy = YToCellY(y + radius);

        if (!TOROIDAL)
        {
            mincx = std::max(mincx, 0);
            maxcx = std::min(maxcx, (int)CELLSX - 1);
            mincy = std::max(mincy, 0);
            maxcy = std::min(maxcy, (int)CELLSY - 1);
        }

        for (int iy = mincy; iy <= maxcy; ++iy)
        {
            int cy = (iy + CELLSY) % CELLSY;

            for (int ix = mincx; ix <= maxcx; ++ix)
            {
                int cx = (ix + CELLSX) % CELLSX;

                for (const auto& p : m_cells[cx][cy])
                {
                    if (TOROIDAL)
                    {
                        if (ToroidalDistanceSq(Vec2{ x, y }, Vec2{ p.x, p.y }) < radius * radius)
                        {
                            results.push_back(p.index);
                            if (stopAfterFirst)
                                return;
                        }
                    }
                    else
                    {
                        if (DistanceSq(Vec2{ x, y }, Vec2{ p.x, p.y }) < radius * radius)
                        {
                            results.push_back(p.index);
                            if (stopAfterFirst)
                                return;
                        }
                    }
                }
            }
        }
    }

    template <bool TOROIDAL>
    void GetPointDistancesSq(float x, float y, float radius, std::vector<float>& results, bool append = true) const
    {
        if (!append)
            results.clear();

        int mincx = XToCellX(x - radius);
        int maxcx = XToCellX(x + radius);
        int mincy = YToCellY(y - radius);
        int maxcy = YToCellY(y + radius);

        if (!TOROIDAL)
        {
            mincx = std::max(mincx, 0);
            maxcx = std::min(maxcx, (int)CELLSX - 1);
            mincy = std::max(mincy, 0);
            maxcy = std::min(maxcy, (int)CELLSY - 1);
        }

        for (int iy = mincy; iy <= maxcy; ++iy)
        {
            int cy = (iy + CELLSY) % CELLSY;

            for (int ix = mincx; ix <= maxcx; ++ix)
            {
                int cx = (ix + CELLSX) % CELLSX;

                for (const auto& p : m_cells[cx][cy])
                {
                    float distanceSq;
                    if (TOROIDAL)
                        distanceSq = ToroidalDistanceSq(Vec2{ x, y }, Vec2{ p.x, p.y });
                    else
                        distanceSq = DistanceSq(Vec2{ x, y }, Vec2{ p.x, p.y });
                    if (distanceSq < radius * radius)
                        results.push_back(distanceSq);
                }
            }
        }
    }

    void AddPoint(int index, float x, float y)
    {
        int cx = XToCellX(x);
        int cy = YToCellY(y);
        m_cells[cx][cy].push_back({ index, x, y });
    }

    void RemovePoint(int index)
    {
        // not super efficient but shrug
        for (auto& col : m_cells)
        {
            for (auto& cell : col)
            {
                for (int i = (int)cell.size() - 1; i >= 0; --i)
                {
                    if (cell[i].index == index)
                        cell.erase(cell.begin() + i);
                    else if (cell[i].index > index)  // need to keep our indices valid
                        cell[i].index--;
                }
            }
        }
    }

private:

    struct PointInternal
    {
        int index;
        float x;
        float y;
    };

    typedef std::vector<PointInternal> TCell;

    std::vector<std::vector<TCell>> m_cells;

};
