#include <cstddef>
#include <cstdio>

struct Summary {
    double total;
    unsigned count;
};

void add(Summary& summary, double value)
{
    summary.total += value;
    ++summary.count;
}

void add(Summary& summary, const double* values, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i) {
        add(summary, values[i]);
    }
}

int main()
{
    Summary summary = {0.0, 0};
    double values[] = {12.0, 18.0, 15.0};

    add(summary, values, sizeof values / sizeof values[0]);

    std::printf("Count: %u, total: %.1f\n", summary.count, summary.total);
    return 0;
}
