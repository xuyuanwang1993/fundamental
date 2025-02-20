

#include "fundamental/basic/buffer.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/basic/utils.hpp"
#include "fundamental/rttr_handler/binary_packer.h"

#include <iostream>
#include <list>
#include <map>
#include <nlohmann/json.hpp>
#include <rttr/registration>
#include <rttr/type>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace rttr;

static nlohmann::json json_from_string(const std::string& str, bool& ok) {
    try {
        ok = true;
        return nlohmann::json::parse(str);
    } catch (const std::exception& e) {
        ok = false;
    }
    return {};
}

static std::string json_to_string(const nlohmann::json& object, bool& ok) {
    ok = true;
    return object.dump(4);
}

enum class color {
    red,
    green,
    blue
};

enum class unregister_color {
    n_red,
    n_green,
    n_blue
};

struct point2d {
    point2d() {
    }
    point2d(int x_, int y_) : x(x_), y(y_) {
    }
    bool operator==(const point2d& other) const noexcept {
        return other.x == x && other.y == y;
    }
    bool operator!=(const point2d& other) const noexcept {
        return other.x != x || other.y != y;
    }
    int x = 0;
    int y = 0;
};
struct ComplexS1 {
    virtual ~ComplexS1() {
    }
    float z;
    std::string s;
    RTTR_ENABLE()
};

struct ComplexS : ComplexS1 {
    color c = color::red;
    point2d point;
    std::vector<point2d> ponits;
    RTTR_ENABLE(ComplexS1)
};

struct ComplexSS : ComplexS {
    std::set<int> dic;
    nlohmann::json object;

    RTTR_ENABLE(ComplexS)
};

struct CustomObject {
    std::vector<color> cs;
    std::vector<nlohmann::json> objects;
    std::list<int> is;

    bool operator==(const CustomObject& other) const noexcept {
        return cs == other.cs && objects == other.objects && is == other.is;
    }
    bool operator!=(const CustomObject& other) const noexcept {
        return cs != other.cs || objects != other.objects || is != other.is;
    }
};

struct CustomObject2 {
    std::list<int> is;
    bool operator==(const CustomObject& other) const noexcept {
        return is == other.is;
    }
    bool operator!=(const CustomObject& other) const noexcept {
        return is != other.is;
    }
};

RTTR_REGISTRATION {
    rttr::registration::class_<point2d>("point2d")
        .constructor()(rttr::policy::ctor::as_object)
        .property("x", &point2d::x)
        .property("y", &point2d::y);
    type::register_converter_func(json_from_string);
    type::register_converter_func(json_to_string);
    rttr::registration::enumeration<color>("color")(value("red", color::red), value("blue", color::blue),
                                                    value("green", color::green));
    {
        using register_type = ComplexS1;
        rttr::registration::class_<register_type>("ComplexS1")
            .constructor()(rttr::policy::ctor::as_object)
            .property("s", &register_type::s)
            .property("z", &register_type::z);
    }
    {
        using register_type = ComplexS;
        rttr::registration::class_<register_type>("ComplexS")
            .constructor()(rttr::policy::ctor::as_object)
            .property("c", &register_type::c)
            .property("point", &register_type::point)
            .property("ponits", &register_type::ponits);
    }
    {
        using register_type = ComplexSS;
        rttr::registration::class_<register_type>("ComplexSS")
            .constructor()(rttr::policy::ctor::as_object)
            .property("dic", &register_type::dic)
            .property("object", &register_type::object);
    }
    {
        using register_type = CustomObject;
        rttr::registration::class_<register_type>("CustomObject")
            .constructor()(rttr::policy::ctor::as_object)
            .property("cs", &register_type::cs)
            .property("is", &register_type::is)
            .property("objects", &register_type::objects);
    }
    {
        using register_type = CustomObject2;
        rttr::registration::class_<register_type>("CustomObject2")
            .constructor()(rttr::policy::ctor::as_object)
            .property("is", &register_type::is);
    }
}
int main(int argc, char* argv[]) {
    using namespace Fundamental::io;

    {
        int a            = 2;
        int b            = 3;
        int c            = 4;
        using tuple_type = std::tuple<int, int, int>;
        tuple_type origin { a, b, c };
        auto data  = binary_batch_pack(a, b, c);
        auto data2 = binary_pack_tuple(origin);
        FINFOS << "gen buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        FINFOS << "gen buf:" << Fundamental::Utils::BufferToHex(data2.data(), data2.size());
        tuple_type gen;

        FASSERT(binary_unpack_tuple(data.data(), data.size(), gen, true, 0) == true);
        FASSERT(origin == gen);
        tuple_type gen2;
        FASSERT(binary_unpack_tuple(data2.data(), data2.size(), gen2, true, 0) == true);
        FASSERT(origin == gen2);
        int a_c = 0;
        int b_c = 0;
        int c_c = 0;
        FASSERT(binary_bacth_unpack(data.data(), data.size(), true, 0, a_c, b_c, c_c) == true);
        FASSERT(a == a_c && b == b_c && c == c_c);
        a_c = 0;
        b_c = 0;
        c_c = 0;
        FASSERT(binary_bacth_unpack(data2.data(), data2.size(), true, 0, a_c, b_c, c_c) == true);
        FASSERT(a == a_c && b == b_c && c == c_c);
    }

    {
        int x     = 2;
        int y     = 3;
        auto data = binary_pack(x);
        binary_pack(data, y);
        FINFOS << "gen buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());

        int x_gen = 0;
        int y_gen = 0;

        FINFOS << binary_unpack(data.data(), data.size(), x_gen, true, 0);
        FINFOS << binary_unpack(data.data(), data.size(), y_gen, true, 1);
        FASSERT(x == x_gen);
        FASSERT(y == y_gen);
    }

    {
        CustomObject origin;
        origin.cs.push_back(color::blue);
        auto& object = origin.objects.emplace_back();
        object["1"]  = 1;
        object["2"]  = 3;
        origin.is.push_back(1);
        origin.is.push_back(2);
        auto data = binary_pack(origin);
        FINFOS << "gen buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        CustomObject gen;
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FASSERT(origin == gen);
        CustomObject2 gen2;
        FINFOS << binary_unpack(data.data(), data.size(), gen2);
        FASSERT(origin.is == gen2.is);
    }

    {
        nlohmann::json origin;
        origin["1"] = 1;
        origin["2"] = 2;
        auto data   = binary_pack(origin);
        FINFOS << "gen buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        nlohmann::json gen;
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FASSERT(origin == gen);
    }

    {
        using basic_type  = bool;
        basic_type origin = false;
        auto data         = binary_pack(origin);
        FINFOS << "buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        basic_type gen {};
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FINFOS << gen;
    }
    {
        using basic_type  = char;
        basic_type origin = 1;
        auto data         = binary_pack(origin);
        FINFOS << "buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        basic_type gen {};
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FINFOS << static_cast<std::uint64_t>(gen);
    }
    {
        using basic_type  = std::int8_t;
        basic_type origin = 1;
        auto data         = binary_pack(origin);
        FINFOS << "buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        basic_type gen {};
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FINFOS << static_cast<std::uint64_t>(gen);
    }
    {
        using basic_type  = std::int16_t;
        basic_type origin = 5;
        auto data         = binary_pack(origin);
        FINFOS << "buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        basic_type gen {};
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FINFOS << static_cast<std::uint64_t>(gen);
    }
    {
        using basic_type  = std::int32_t;
        basic_type origin = 5;
        auto data         = binary_pack(origin);
        FINFOS << "buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        basic_type gen {};
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FINFOS << gen;
    }
    {
        using basic_type  = std::int64_t;
        basic_type origin = 5;
        auto data         = binary_pack(origin);
        FINFOS << "buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        basic_type gen {};
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FINFOS << gen;
    }
    {
        using basic_type  = std::uint8_t;
        basic_type origin = 5;
        auto data         = binary_pack(origin);
        FINFOS << "buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        basic_type gen {};
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FINFOS << static_cast<std::uint64_t>(gen);
    }
    {
        using basic_type  = std::uint16_t;
        basic_type origin = 5;
        auto data         = binary_pack(origin);
        FINFOS << "buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        basic_type gen {};
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FINFOS << static_cast<std::uint64_t>(gen);
    }
    {
        using basic_type  = std::uint32_t;
        basic_type origin = 5;
        auto data         = binary_pack(origin);
        FINFOS << "buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        basic_type gen {};
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FINFOS << gen;
    }
    {
        using basic_type  = std::uint64_t;
        basic_type origin = 5;
        auto data         = binary_pack(origin);
        FINFOS << "buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        basic_type gen {};
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FINFOS << gen;
    }
    {
        using basic_type  = float;
        basic_type origin = 5;
        auto data         = binary_pack(origin);
        FINFOS << "buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        basic_type gen {};
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FINFOS << gen;
    }
    {
        using basic_type  = double;
        basic_type origin = 5;
        auto data         = binary_pack(origin);
        FINFOS << "buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        basic_type gen {};
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FINFOS << gen;
    }

    {
        using basic_type  = std::string;
        basic_type origin = "123";
        auto data         = binary_pack(origin);
        FINFOS << "buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        basic_type gen;
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FINFOS << gen;
    }

    {
        using basic_type  = color;
        basic_type origin = color::green;
        auto data         = binary_pack(origin);
        FINFOS << "buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        basic_type gen;
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FINFOS << static_cast<std::uint64_t>(gen);
        unregister_color n_c;
        FASSERT(!binary_unpack(data.data(), data.size(), n_c));
    }

    {
        using basic_type = std::vector<int>;
        basic_type origin { 0, 1, 2, 3, 4 };
        auto data = binary_pack(origin);
        FINFOS << "buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        basic_type gen;
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FASSERT(gen == origin);
    }
    {
        using basic_type = std::list<int>;
        basic_type origin { 0, 1, 2, 3, 4 };
        auto data = binary_pack(origin);
        FINFOS << "buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        basic_type gen;
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FASSERT(gen == origin);
    }

    {
        using basic_type = std::set<int>;
        basic_type origin { 0, 1, 2, 3, 4 };
        auto data = binary_pack(origin);
        FINFOS << "buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        basic_type gen;
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FASSERT(gen == origin);
    }

    {
        using basic_type = std::map<int, int>;
        basic_type origin { { 0, 0 }, { 1, 1 }, { 2, 2 }, { 3, 3 }, { 4, 4 } };
        auto data = binary_pack(origin);
        FINFOS << "buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        basic_type gen;
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FASSERT(gen == origin);
    }

    {
        using basic_type = std::unordered_set<int>;
        basic_type origin { 0, 1, 2, 3, 4 };
        auto data = binary_pack(origin);
        FINFOS << "buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        basic_type gen;
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FASSERT(gen == origin);
    }
    {
        using basic_type = std::unordered_map<std::string, std::string>;
        basic_type origin { { "0", "0" }, { "1", "1" }, { "2", "2" }, { "3", "3" }, { "4", "4" } };
        auto data = binary_pack(origin);
        FINFOS << "buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        basic_type gen;
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FASSERT(gen == origin);
    }

    {
        point2d origin { 2, 3 };
        auto data = binary_pack(origin);
        FINFOS << "buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        point2d gen;
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FINFOS << gen.x;
        FINFOS << gen.y;
    }
    {
        ComplexS1 origin;
        origin.z  = 0.1f;
        origin.s  = "231232132";
        auto data = binary_pack(origin);
        FINFOS << "gen buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        ComplexS1 gen;
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FASSERT(origin.s == gen.s);
        FASSERT(origin.z == gen.z);
    }

    {
        ComplexSS origin;
        origin.ponits      = decltype(origin.ponits) { { 1, 2 }, { 3, 4 }, { 5, 6 } };
        origin.dic         = decltype(origin.dic) { 1, 2, 3, 4, 5 };
        origin.c           = color::blue;
        origin.point       = { 4, 5 };
        origin.object["1"] = 1;
        origin.object["2"] = 2;
        auto data          = binary_pack(origin);
        FINFOS << "gen buf:" << Fundamental::Utils::BufferToHex(data.data(), data.size());
        ComplexSS gen;
        FINFOS << binary_unpack(data.data(), data.size(), gen);
        FASSERT(origin.point == gen.point);
        FASSERT(origin.dic == gen.dic);
        FASSERT(origin.ponits == gen.ponits);
        FASSERT(origin.object == gen.object);
        FASSERT(static_cast<int>(origin.c) == static_cast<int>(gen.c));
    }

    return 0;
}
