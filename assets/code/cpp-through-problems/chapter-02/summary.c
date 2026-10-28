#include <stdio.h>

struct Summary {
    double total;
    unsigned count;
};

void add(struct Summary *summary, double value)
{
    summary->total += value;
    ++summary->count;
}

int main(void)
{
    struct Summary summary = {0.0, 0};
    double values[] = {12.0, 18.0, 15.0};
    unsigned i;

    for (i = 0; i < 3; ++i) {
        add(&summary, values[i]);
    }

    printf("Count: %u, total: %.1f\n", summary.count, summary.total);
    return 0;
}
