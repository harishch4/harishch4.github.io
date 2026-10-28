#include <iostream>
#include <vector>

int main()
{
    std::vector<int> samples{12, 18, 15};
    samples.push_back(21);

    int total = 0;
    for (int sample : samples) {
        total += sample;
    }

    std::cout << "Total: " << total << '\n';
}
