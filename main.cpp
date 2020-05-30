#include <getopt.h>
#include <ncurses.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

namespace {
const int end_of_transmission = 4;
const int escape = 27;

namespace color {
    bool use_color = true;
    enum class type {
        label = 1,
        value,
        ok,
        warn,
        bad
    };
}

void set_color(color::type color)
{
    if (color::use_color) {
        attron(COLOR_PAIR(static_cast<int>(color)));
    }
}

void remove_color(color::type color)
{
    if (color::use_color) {
        attroff(COLOR_PAIR(static_cast<int>(color)));
    }
}

void print_string(color::type color, const std::string &str, int attr = 0)
{
    attron(attr);
    set_color(color);
    addstr(str.c_str());
    remove_color(color);
    attroff(attr);
}

class device {
public:
    device(std::string_view path)
        : m_path(path)
    {
        m_vram = std::stoull(read_file("mem_info_vram_total"));
        m_vram_str = '/' + std::to_string(m_vram / (1024ull * 1024ull)) + "MiB";

        m_gtt = std::stoull(read_file("mem_info_gtt_total"));
        m_gtt_str = '/' + std::to_string(m_gtt / (1024ull * 1024ull)) + "MiB";

        m_vis_vram = std::stoull(read_file("mem_info_vis_vram_total"));
        m_vis_vram_str = '/' + std::to_string(m_vis_vram / (1024ull * 1024ull)) + "MiB";

        m_power_min = std::stoull(read_file("hwmon/hwmon1/power1_cap_min"));
        m_power_max = std::stoull(read_file("hwmon/hwmon1/power1_cap_max"));

        m_temp_crit = std::stoull(read_file("hwmon/hwmon1/temp1_crit"));

        m_fan_min = std::stoull(read_file("hwmon/hwmon1/fan1_min"));
        m_fan_max = std::stoull(read_file("hwmon/hwmon1/fan1_max"));
    }

    std::pair<std::string, double> busy() const
    {
        auto pc = read_file("gpu_busy_percent");
        return std::make_pair(pc + '%', std::stod(pc) * 0.01);
    }

    std::pair<std::string, double> vram() const
    {
        auto used = read_file("mem_info_vram_used");
        auto u = std::stoull(used);
        auto pc = static_cast<double>(u) / static_cast<double>(m_vram);
        u /= 1024ull * 1024ull;

        used = std::to_string(u) + m_vram_str;
        return std::make_pair(used, pc);
    }

    std::pair<std::string, double> gtt() const
    {
        auto used = read_file("mem_info_gtt_used");
        auto u = std::stoull(used);
        auto pc = static_cast<double>(u) / static_cast<double>(m_gtt);
        u /= 1024ull * 1024ull;

        used = std::to_string(u) + m_gtt_str;
        return std::make_pair(used, pc);
    }

    std::pair<std::string, double> vis_vram() const
    {
        auto used = read_file("mem_info_vis_vram_used");
        auto u = std::stoull(used);
        auto pc = static_cast<double>(u) / static_cast<double>(m_vis_vram);
        u /= 1024ull * 1024ull;

        used = std::to_string(u) + m_vis_vram_str;
        return std::make_pair(used, pc);
    }

    std::pair<std::string, double> power() const
    {
        auto pwr = read_file("hwmon/hwmon1/power1_average");
        auto p = std::stoull(pwr);
        auto range = static_cast<double>(m_power_max - m_power_min);
        auto pc = static_cast<double>(p - m_power_min) / range;

        return std::make_pair(std::to_string(p / 1000000ull) + 'W', pc);
    }

    std::pair<std::string, double> temperature() const
    {
        auto temp = read_file("hwmon/hwmon1/temp1_input");
        auto t = std::stoull(temp);
        auto pc = static_cast<double>(t) / static_cast<double>(m_temp_crit);

        return std::make_pair(std::to_string(t / 1000ull) + 'C', pc);
    }

    std::pair<std::string, double> fan() const
    {
        auto f = read_file("hwmon/hwmon1/fan1_input");
        auto range = static_cast<double>(m_fan_max - m_fan_min);
        auto pc = static_cast<double>(std::stod(f) - m_fan_min) / range;

        return std::make_pair(f + "RPM", pc);
    }

    std::string voltage() const
    {
        return read_file("hwmon/hwmon1/in0_input") + "mV";
    }

    std::string gfx_clock() const
    {
        auto freq = read_file("hwmon/hwmon1/freq1_input");
        auto f = std::stoull(freq);
        f /= 1000000ull;
        return std::to_string(f) + "MHz";
    }

    std::string mem_clock() const
    {
        auto freq = read_file("hwmon/hwmon1/freq2_input");
        auto f = std::stoull(freq);
        f /= 1000000ull;
        return std::to_string(f) + "MHz";
    }

    std::string link_speed() const
    {
        return read_file("current_link_speed");
    }

    std::string link_width() const
    {
        return 'x' + read_file("current_link_width");
    }

private:
    std::string read_file(const std::string_view path) const
    {
        auto file = m_path;
        file.append(path);

        std::ifstream input(file);
        if (!input.is_open()) {
            return "0";
        }

        std::string ret;
        std::getline(input, ret);
        return ret;
    }

    std::string m_path;
    std::string m_vram_str;
    std::string m_gtt_str;
    std::string m_vis_vram_str;

    using ull = unsigned long long;

    ull m_vram;
    ull m_gtt;
    ull m_vis_vram;
    ull m_power_min;
    ull m_power_max;
    ull m_temp_crit;
    ull m_fan_min;
    ull m_fan_max;
};

void draw_bar(int row, int col, int width, double pc, const std::string &str)
{
    move(row, col);
    clrtoeol();

    pc = std::clamp(pc, 0.0, 1.0);
    width -= 2 + static_cast<int>(str.size());

    auto bars = static_cast<int>(width * pc);
    if (bars < 0) {
        return;
    }

    attron(A_BOLD);
    addch('[');
    attroff(A_BOLD);

    auto bar_color =
        pc < 0.33 ? color::type::ok :
        pc < 0.67 ? color::type::warn :
        color::type::bad;

    std::string bar(static_cast<size_t>(bars), '|');
    print_string(bar_color, bar);

    move(row, col + width + 1);

    attron(A_BOLD);
    print_string(color::type::value, str);
    addch(']');
    attroff(A_BOLD);
}

const int vpad = 1;
const int hpad = 2;

void draw_labels()
{
    int row = vpad-1;

    set_color(color::type::label);
    mvaddstr(++row, hpad, "GPU busy:");
    mvaddstr(++row, hpad, "GPU vram:");
    mvaddstr(++row, hpad, "GTT:");
    mvaddstr(++row, hpad, "CPU Vis:");
    mvaddstr(++row, hpad, "Power draw:");
    mvaddstr(++row, hpad, "Temperature:");
    mvaddstr(++row, hpad, "Fan speed:");
    mvaddstr(++row, hpad, "Voltage:");
    mvaddstr(++row, hpad, "GFX clock:");
    mvaddstr(++row, hpad, "Mem clock:");
    mvaddstr(++row, hpad, "Link speed:");
    mvaddstr(++row, hpad, "Link width:");
    remove_color(color::type::label);
}

void printHelp(std::string_view progName)
{
    std::cout << "Usage: " << progName << " [options]\n"
        "Released under the GNU GPLv3\n\n"
        "  -n, --no-color  disable colors\n"
        "  -u, --update=N  set automatic updates to N seconds (default 2)\n"
        "  -h, --help      display this message\n";
}

void handle_winch()
{
    winsize w;
    ioctl(0, TIOCGWINSZ, &w);
    resizeterm(w.ws_row, w.ws_col);
    clear();
    draw_labels();
}

volatile sig_atomic_t should_close = 0;
volatile sig_atomic_t should_resize = 0;
void signal_handler(int sig)
{
    switch (sig) {
    case SIGINT:
    case SIGTERM:
        should_close = 1;
        break;
    case SIGWINCH:
        should_resize = 1;
        break;
    }
}
}

int main(int argc, char **argv)
{
    const option options[] = {
        {"update", required_argument, nullptr, 'u'},
        {"no-color", no_argument, nullptr, 'n'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };

    int sleep_time = 2;

    int c;
    while ((c = getopt_long(argc, argv, "hnu:", options, nullptr)) != -1) {
        switch (c) {
        case 'h':
            printHelp(argv[0]);
            return EXIT_SUCCESS;
        case 'n':
            color::use_color = false;
            break;
        case 'u':
            sleep_time = std::stoi(optarg);
            break;
        default:
            return EXIT_FAILURE;
        }
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGWINCH, signal_handler);

    initscr();

    timeout(sleep_time*1000);
    noecho();
    curs_set(0);
    keypad(stdscr, true);
    clear();

    color::use_color &= has_colors();

    if (color::use_color) {
        start_color();
        use_default_colors();
        init_pair(static_cast<int>(color::type::label), COLOR_CYAN, -1);
        init_pair(static_cast<int>(color::type::value), COLOR_BLACK, -1);
        init_pair(static_cast<int>(color::type::ok), COLOR_GREEN, -1);
        init_pair(static_cast<int>(color::type::warn), COLOR_YELLOW, -1);
        init_pair(static_cast<int>(color::type::bad), COLOR_RED, -1);
    }

    draw_labels();

    const auto text_len = 13 + hpad;

    const device dev("/sys/class/drm/card0/device/");

    while (!should_close) {
        if (should_resize) {
            handle_winch();
            should_resize = 0;
        }

        int bar_width = COLS - text_len - hpad;
        int row = vpad-1;

        auto [busy, busy_pc] = dev.busy();
        draw_bar(++row, text_len, bar_width, busy_pc, busy);

        auto [mem, mem_pc] = dev.vram();
        draw_bar(++row, text_len, bar_width, mem_pc, mem);

        auto [gtt, gtt_pc] = dev.gtt();
        draw_bar(++row, text_len, bar_width, gtt_pc, gtt);

        auto [vis, vis_pc] = dev.vis_vram();
        draw_bar(++row, text_len, bar_width, vis_pc, vis);

        auto [pwr, pwr_pc] = dev.power();
        draw_bar(++row, text_len, bar_width, pwr_pc, pwr);

        auto [temp, temp_pc] = dev.temperature();
        draw_bar(++row, text_len, bar_width, temp_pc, temp);

        auto [fan, fan_pc] = dev.fan();
        draw_bar(++row, text_len, bar_width, fan_pc, fan);

        move(++row, text_len);
        clrtoeol();
        print_string(color::type::label, dev.voltage(), A_BOLD);

        move(++row, text_len);
        clrtoeol();
        print_string(color::type::label, dev.gfx_clock(), A_BOLD);

        move(++row, text_len);
        clrtoeol();
        print_string(color::type::label, dev.mem_clock(), A_BOLD);

        move(++row, text_len);
        clrtoeol();
        print_string(color::type::label, dev.link_speed(), A_BOLD);

        move(++row, text_len);
        clrtoeol();
        print_string(color::type::label, dev.link_width(), A_BOLD);

        refresh();

        auto key = getch();
        if (key == 'q' || key == end_of_transmission || key == escape) {
            break;
        }
    }

    endwin();

    return EXIT_SUCCESS;
}
