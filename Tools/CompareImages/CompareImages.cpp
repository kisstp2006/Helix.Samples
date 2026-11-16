#include <Helix/Core/Strings/Format.h>
#include <Helix/Image/Codecs/Png.h>
#include <Helix/Platform/IO/FileSystem.h>

#include <cstdio>
#include <cstdlib>

// Compares a screenshot with a golden image: CompareImages <golden.png> <shot.png> [tolerance] [share]. Pixels whose
// channels differ by more than the tolerance (8) are counted; more than the share (0.001) of them fails. With
// HELIX_UPDATE_GOLDEN=1 the screenshot becomes the golden image. The exit code is 0 when they match.
using namespace helix;

namespace
{
    bool Load(const char* path, image::Image& out)
    {
        Array<u8> file;
        return platform::ReadFile(StringView(path), file) && image::DecodePng(file, out) && out.format == image::ImageFormat::RGBA8;
    }
}

int main(int count, char** args)
{
    if (count < 3)
    {
        std::printf("CompareImages <golden.png> <shot.png> [tolerance] [share]\n");
        return 2;
    }
    const char* update = std::getenv("HELIX_UPDATE_GOLDEN");
    if (update && update[0] == '1')
    {
        Array<u8> file;
        if (!platform::ReadFile(StringView(args[2]), file) || !platform::WriteFile(StringView(args[1]), file))
        {
            std::printf("%s could not be written\n", args[1]);
            return 1;
        }
        std::printf("golden %s written\n", args[1]);
        return 0;
    }
    i32 tolerance = count > 3 ? std::atoi(args[3]) : 8;
    f64 share = count > 4 ? std::atof(args[4]) : 0.001;
    image::Image golden, shot;
    if (!Load(args[1], golden))
    {
        std::printf("golden %s missing or not RGBA8, run with HELIX_UPDATE_GOLDEN=1\n", args[1]);
        return 1;
    }
    if (!Load(args[2], shot))
    {
        std::printf("screenshot %s missing or not RGBA8\n", args[2]);
        return 1;
    }
    if (golden.width != shot.width || golden.height != shot.height)
    {
        std::printf("sizes differ: golden %ux%u, screenshot %ux%u\n", golden.width, golden.height, shot.width, shot.height);
        return 1;
    }
    u64 different = 0;
    i32 worst = 0;
    for (usize i = 0; i < golden.pixels.Size(); i += 4)
    {
        i32 most = 0;
        for (usize c = 0; c < 3; c++)
        {
            i32 d = i32(golden.pixels[i + c]) - i32(shot.pixels[i + c]);
            most = d < 0 ? (-d > most ? -d : most) : (d > most ? d : most);
        }
        worst = most > worst ? most : worst;
        different += most > tolerance ? 1 : 0;
    }
    u64 total = u64(golden.width) * golden.height;
    bool ok = f64(different) <= share * f64(total);
    std::printf("%s: %llu of %llu pixels differ by more than %d (largest difference %d)\n", ok ? "match" : "MISMATCH", (unsigned long long)different,
                (unsigned long long)total, tolerance, worst);
    return ok ? 0 : 1;
}
