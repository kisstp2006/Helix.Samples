#include <Helix/Samples/Launcher/Launcher.h>

#include <Helix/Core/Containers/Array.h>

#if defined(_WIN32)
#include <windows.h>
#endif

// The launcher's entry: a console program (its log shows the reloads), the arguments as UTF-8.
#if defined(_WIN32)
int wmain(int count, wchar_t** wide)
{
    helix::Array<helix::String> args;
    for (int i = 1; i < count; i++)
    {
        int n = WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, nullptr, 0, nullptr, nullptr);
        helix::Array<char> utf8;
        utf8.Resize(helix::usize(n > 0 ? n : 1));
        WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, utf8.Data(), n, nullptr, nullptr);
        args.Push(helix::String(helix::StringView(utf8.Data(), helix::usize(n > 0 ? n - 1 : 0))));
    }
#else
int main(int count, char** argv)
{
    helix::Array<helix::String> args;
    for (int i = 1; i < count; i++)
        args.Push(helix::String(helix::StringView(argv[i])));
#endif
    helix::Array<helix::StringView> views;
    for (const helix::String& a : args)
        views.Push(a.View());
    return helix::samples::RunLauncher(helix::Span<const helix::StringView>(views.Data(), views.Size()));
}
