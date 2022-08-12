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
        return int(x * float(CELLSX - 1) + 0.5f);
    }

    static const int YToCellY(float y)
    {
        return int(y * float(CELLSY - 1) + 0.5f);
    }

    void GetPoints(float x, float y, float radius, std::vector<int>& results, bool append = true) const
    {
        if (!append)
            results.clear();

        int mincx = XToCellX(x - radius);
        int maxcx = XToCellX(x + radius);
        int mincy = YToCellY(y - radius);
        int maxcy = YToCellY(y + radius);

        for (int iy = mincy; iy <= maxcy; ++iy)
        {
            int cy = (iy + CELLSY) % CELLSY;

            for (int ix = mincx; ix <= maxcx; ++ix)
            {
                int cx = (ix + CELLSX) % CELLSX;

                for (const auto& p : m_cells[cx][cy])
                {
                    if (ToroidalDistance(Vec2{ x, y }, Vec2{ p.x, p.y }) < radius)
                        results.push_back(p.index);
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
                for (size_t i = 0; i < cell.size(); ++i)
                {
                    if (cell[i].index == index)
                    {
                        cell.erase(cell.begin() + i);
                        return;
                    }
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
