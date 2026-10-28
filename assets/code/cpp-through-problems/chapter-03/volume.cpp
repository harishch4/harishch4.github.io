#include <cstdio>
#include <stdexcept>

class Volume {
private:
    int level_;

public:
    explicit Volume(int initial = 0) : level_(initial)
    {
        if (initial < 0 || initial > 100) {
            throw std::out_of_range("Volume must be between 0 and 100");
        }
    }

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
    Volume muted;
    Volume music(40);
    const Volume reference(25);

    std::printf("Default: %d\n", muted.level());
    std::printf("Initial music: %d\n", music.level());
    music.set(50);
    std::printf("Updated music: %d\n", music.level());
    std::printf("Const object: %d\n", reference.level());

    try {
        Volume invalid(150);
    } catch (const std::out_of_range& error) {
        std::printf("Construction failed: %s\n", error.what());
    }
}
