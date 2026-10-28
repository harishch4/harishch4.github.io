#include <cstdio>

class Volume {
private:
    int level_;

public:
    Volume() : level_(0) {}

    bool set(int value)
    {
        if (value < 0 || value > 100) {
            return false;
        }
        level_ = value;
        return true;
    }

    int level() const
    {
        return level_;
    }
};

int main()
{
    Volume volume;
    std::printf("Initial: %d\n", volume.level());

    volume.set(50);
    std::printf("After setting 50: %d\n", volume.level());

    if (!volume.set(150)) {
        std::printf("Rejected 150; still: %d\n", volume.level());
    }
}

