

#include "fundamental/basic/log.h"
#include "fundamental/rttr_handler/deserializer.h"
#include "fundamental/rttr_handler/serializer.h"
#include "fundamental/delay_queue/delay_queue.h"
#include "TestRttrRegister.h"
#include <rttr/registration>
#include <iostream>
using namespace rttr;

enum class color
{
    red,
    green,
    blue
};

struct point2d
{
    point2d()
    {
    }
    point2d(int x_, int y_) :
    x(x_), y(y_)
    {
    }
    int x = 0;
    int y = 0;
};

struct shape
{
    shape()
    {
    }
    shape(std::string n) :
    name(n)
    {
    }

    void set_visible(bool v)
    {
        visible = v;
    }
    bool get_visible() const
    {
        return visible;
    }

    color color_     = color::blue;
    std::string name = "";
    point2d position;
    std::map<color, point2d> dictionary;

    RTTR_ENABLE()
private:
    bool visible = false;
};

struct circle : shape
{
    circle(std::string n) :
    shape(n)
    {
    }

    double radius = 5.2;
    std::vector<point2d> points;

    int no_serialize = 100;

    RTTR_ENABLE(shape)
};

struct vecTest
{
    std::vector<float> color;
};
struct MapTest
{
    std::map<std::string, vecTest> test;
};
struct CombineData
{
    point2d point;
    vecTest vec;
    MapTest map;
};

struct VecTest1
{
    int a;
    std::string b;
};
struct JsonTest
{
    nlohmann::json test;
    std::vector<VecTest1> vecTest;
};

struct CommonGroupData
{
    std::size_t commonInt = 0;
    std::string commonStr  = "default common";
};

struct LocalGroupData
{
    std::size_t localInt = 0;
    std::string localStr  = "default local";
};

struct CloudGroupData
{
    std::size_t cloudInt = 0;
    std::string cloudStr  = "default cloud";
};

struct TestSerializeControl
{
    CommonGroupData data;
    LocalGroupData localData;
    CloudGroupData cloudData;
    std::vector<LocalGroupData> localDataVec;
    std::map<std::size_t, CloudGroupData> cloudDataMap;
};

struct TestReferenceBind
{
    std::string data;
};

struct TestUnReferenceBind
{
    std::string data;
};
RTTR_REGISTRATION
{

    rttr::registration::class_<shape>("shape")
        .constructor()(rttr::policy::ctor::as_object)
        .property("visible", &shape::get_visible, &shape::set_visible)
        .property("color", &shape::color_)
        .property("name", &shape::name)
        .property("position", &shape::position)
        .property("dictionary", &shape::dictionary);

    rttr::registration::class_<circle>("circle")
        .property("radius", &circle::radius)
        .property("points", &circle::points)
        .property("no_serialize", &circle::no_serialize)(
            metadata("NO_SERIALIZE", true));

    rttr::registration::class_<point2d>("point2d")
        .constructor()(rttr::policy::ctor::as_object)
        .property("x", &point2d::x)
        .property("y", &point2d::y);

    rttr::registration::enumeration<color>("color")(
        value("red", color::red),
        value("blue", color::blue),
        value("green", color::green));
    rttr::registration::class_<vecTest>("vecTest")
        .constructor()(rttr::policy::ctor::as_object)
        .property("color", &vecTest::color);
    rttr::registration::class_<MapTest>("MapTest")
        .constructor()(rttr::policy::ctor::as_object)
        .property("test", &MapTest::test);
    rttr::registration::class_<CombineData>("CombineData")
        .property("point", &CombineData::point)
        .property("vec", &CombineData::vec)
        .property("map", &CombineData::map);

    rttr::registration::class_<VecTest1>("VecTest1")
        .property("a", &VecTest1::a)
        .property("b", &VecTest1::b);

    // registration::class_<std::vector<VecTest1>>("std::vector<VecTest1>");

    rttr::registration::class_<JsonTest>("JsonTest")
        .property("test", &JsonTest::test)
        .property("vecTest", &JsonTest::vecTest);

    {
        using RegisterType = CommonGroupData;
        rttr::registration::class_<RegisterType>("CommonGroupData")
            .property("commonInt", &RegisterType::commonInt)
            .property("commonStr", &RegisterType::commonStr);
    }
    {
        using RegisterType = LocalGroupData;
        rttr::registration::class_<RegisterType>("LocalGroupData")
            .property("localInt", &RegisterType::localInt)
            .property("localStr", &RegisterType::localStr)(
                metadata(Fundamental::RttrMetaControlOption::ExcludeMetaDataKey(), Fundamental::RttrControlMetaDataType { "all" }),
                metadata(Fundamental::RttrMetaControlOption::IncludeMetaDataKey(), Fundamental::RttrControlMetaDataType { "local", "all" }));
    }
    {
        using RegisterType = CloudGroupData;
        rttr::registration::class_<RegisterType>("CloudGroupData")
            .constructor()(rttr::policy::ctor::as_object)
            .property("cloudInt", &RegisterType::cloudInt)
            .property("cloudStr", &RegisterType::cloudStr)(
                metadata(Fundamental::RttrMetaControlOption::ExcludeMetaDataKey(), Fundamental::RttrControlMetaDataType { "all" }),
                metadata(Fundamental::RttrMetaControlOption::IncludeMetaDataKey(), Fundamental::RttrControlMetaDataType { "cloud", "all" }));
    }
    {
        using RegisterType = TestSerializeControl;
        rttr::registration::class_<RegisterType>("TestSerializeControl")
            .constructor()(rttr::policy::ctor::as_object)
            .property("cloudData", &RegisterType::cloudData)
            .property("cloudDataMap", &RegisterType::cloudDataMap)(
                metadata(Fundamental::RttrMetaControlOption::ExcludeMetaDataKey(), Fundamental::RttrControlMetaDataType { "nomap" }),
                metadata(Fundamental::RttrMetaControlOption::IncludeMetaDataKey(), Fundamental::RttrControlMetaDataType { "cloud", "map", "all" }))
            .property("data", &RegisterType::data)
            .property("localData", &RegisterType::localData)
            .property("localDataVec", &RegisterType::localDataVec)(
                metadata(Fundamental::RttrMetaControlOption::ExcludeMetaDataKey(), Fundamental::RttrControlMetaDataType { "novec" }),
                metadata(Fundamental::RttrMetaControlOption::IncludeMetaDataKey(), Fundamental::RttrControlMetaDataType { "cloud", "vec", "all" }));
    }
    {
        using RegisterType = TestReferenceBind;
        rttr::registration::class_<RegisterType>("TestReferenceBind")
            .property("data", &RegisterType::data)(
                policy::prop::as_reference_wrapper);
    }
    {
        using RegisterType = TestUnReferenceBind;
        rttr::registration::class_<RegisterType>("TestUnReferenceBind")
            .property("data", &RegisterType::data);
    }
}

void TestPlugin();
int main(int argc, char** argv)
{
    Fundamental::Logger::TestLogInstance();
    Fundamental::TestRttrInstance();
    if (0)
    {
        FINFO("test rttr meta data");
        TestSerializeControl controlData;
        controlData.data.commonInt     = 1;
        controlData.data.commonStr     = "common test";
        controlData.localData.localInt = 2;
        controlData.localData.localStr = "local test";
        controlData.cloudData.cloudInt = 3;
        controlData.cloudData.cloudStr = "cloud test";
        for (std::size_t i = 1; i < 3; ++i)
        {
            LocalGroupData testGroupData;
            testGroupData.localInt = i;
            testGroupData.localStr = std::string("local vec ") + std::to_string(i);
            controlData.localDataVec.emplace_back(testGroupData);
        }
        for (std::size_t i = 1; i < 3; ++i)
        {
            CloudGroupData testGroupData;
            testGroupData.cloudInt = i;
            testGroupData.cloudStr = std::string("cloud map ") + std::to_string(i);
            controlData.cloudDataMap.try_emplace(i, testGroupData);
        }
        std::string baseStr = Fundamental::io::to_json(controlData);
        FINFO("base data->{}", baseStr);
        {
            FWARN("test only vec");
            Fundamental::RttrMetaControlOption option;
            option.includeDatas.insert("vec");
            {
                FINFO("test only serialize output->{}", Fundamental::io::to_json(controlData,option));
            }
            {
                TestSerializeControl deserializedData;
                Fundamental::io::from_json(baseStr, deserializedData, option);
                FINFO("test only deserialized output->{}", Fundamental::io::to_json(deserializedData));
            }
        }
        {
            FWARN("test only map");
            Fundamental::RttrMetaControlOption option;
            option.includeDatas.insert("map");
            {
                FINFO("test only serialize output->{}", Fundamental::io::to_json(controlData, option));
            }
            {
                TestSerializeControl deserializedData;
                Fundamental::io::from_json(baseStr, deserializedData, option);
                FINFO("test only deserialized output->{}", Fundamental::io::to_json(deserializedData));
            }
        }
        {
            FWARN("test exclude all");
            Fundamental::RttrMetaControlOption option;
            option.excludeDatas.insert("all");
            {
                FINFO("test only serialize output->{}", Fundamental::io::to_json(controlData, option));
            }
            {
                TestSerializeControl deserializedData;
                Fundamental::io::from_json(baseStr, deserializedData, option);
                FINFO("test only deserialized output->{}", Fundamental::io::to_json(deserializedData));
            }
        }
        FWARN("test reference cost");
        std::size_t dataSize = 20LLU * 1024 * 1024;
        {
            using TestType = TestUnReferenceBind;
            FWARN("test no reference cost");
            decltype(TestType::data) setData(dataSize, 'a');
            auto property = type::get<TestType>().get_property("data");
            assert(property.is_valid());
            TestType instance;
            auto timeNow = Fundamental::Timer::GetTimeNow();
            property.set_value(instance, setData);
            FWARN("cost {} ms", Fundamental::Timer::GetTimeNow()-timeNow);
            assert(instance.data.size() == dataSize);
        }
        {
            using TestType = TestReferenceBind;
            FWARN("test  reference cost");
            decltype(TestType::data) setData(dataSize, 'a');
            auto property = type::get<TestType>().get_property("data");
            assert(property.is_valid());
            TestType instance;
            auto timeNow = Fundamental::Timer::GetTimeNow();
            property.set_value(instance, std::ref(setData));
            FWARN("cost {} ms", Fundamental::Timer::GetTimeNow() - timeNow);
            assert(instance.data.size() == dataSize);
        }
        return 0;
    }
    TestPlugin();
    {
        JsonTest t;
        t.test["111"] = 1;
        t.test["222"] = 2;
        t.vecTest.emplace_back();
        t.vecTest[0].a = 1;
        t.vecTest[0].b = "qwqe";
        t.vecTest.emplace_back();
        t.vecTest[1].a   = 2;
        t.vecTest[1].b   = "xxxx";
        std::string abcd = Fundamental::io::to_json(t);
        std::cout << "json-test:" << abcd << std::endl;
        JsonTest v2;
        Fundamental::io::from_json(abcd, v2);

        abcd = Fundamental::io::to_json(v2);
        std::cout << "json-test:" << abcd << std::endl;
    }
    {
        auto type     = type::get_by_name("shape");
        auto dTest    = type.create();
        property prop = type::get(dTest).get_property("radius");
        prop.set_value(dTest, 0.5);

        auto str = Fundamental::io::to_json(dTest);

        std::cout << "dtest:" << str << std::endl;
    }
    std::string json_string;

    {
        enumeration e = type::get_by_name("color").get_enumeration();
        auto testStr  = e.name_to_value("blue"); //.is_valid();
        auto value    = testStr.get_value<color>();
        auto ptr      = e.value_to_name(3).data();
        circle c_1("Circle #1");
        shape& my_shape = c_1;

        c_1.set_visible(true);
        c_1.points      = std::vector<point2d>(2, point2d(1, 1));
        c_1.points[1].x = 23;
        c_1.points[1].y = 42;

        c_1.position.x = 12;
        c_1.position.y = 66;

        c_1.radius = 5.123;
        c_1.color_ = color::red;

        // additional braces are needed for a VS 2013 bug
        c_1.dictionary = { { { color::green, { 1, 2 } }, { color::blue, { 3, 4 } }, { color::red, { 5, 6 } } } };

        c_1.no_serialize = 12345;

        json_string = Fundamental::io::to_json(my_shape); // serialize the circle to 'json_string'

        auto str = Fundamental::io::to_json(c_1.points[0].x);
        std::cout << "circle test:" << str << std::endl;
    }

    std::cout << "Circle: c_1:\n"
              << json_string << std::endl;

    circle c_2("Circle #2"); // create a new empty circle

    Fundamental::io::from_json(json_string, c_2); // deserialize it with the content of 'c_1'
    std::cout << "\n############################################\n"
              << std::endl;

    std::cout << "Circle c_2:\n"
              << Fundamental::io::to_json(c_2) << std::endl;

    {
        vecTest v;
        v.color.emplace_back(0.1f);
        v.color.emplace_back(1.1f);
        v.color.emplace_back(2.1f);
        auto str = Fundamental::io::to_json(v);
        std::cout << "vec test:" << str << std::endl;

        vecTest v2;
        Fundamental::io::from_json(str, v2);
        std::cout << "vec test paser:" << v2.color.size() << std::endl;
        for (auto& i : v2.color)
        {
            std::cout << i << std::endl;
        }
        Fundamental::io::set_property_by_json(v2, "color.1", Fundamental::io::to_json(0.55));
    }
    {
        point2d p;
        p.x       = 0;
        p.y       = 1;
        auto str  = Fundamental::io::to_json(p);
        auto type = type::get_by_name("point2d");

        auto var = Fundamental::io::from_json(str, type);

        std::cout << "point2d:" << var.get_value<point2d>().y << std::endl;
    }
    {
        CombineData data;
        data.point.x = -1;
        data.point.y = -2;
        data.vec.color.emplace_back(10.1f);
        data.vec.color.emplace_back(12.1f);
        data.vec.color.emplace_back(20.1f);
        data.map.test.emplace("1", data.vec);
        data.map.test.emplace("2", data.vec);
        auto str = Fundamental::io::to_json(data);
        std::cout << "CombineData test:" << str << std::endl;
        // test simple property
        Fundamental::io::set_property_by_json(data, "point.x", "-2");
        // test map item
        Fundamental::io::set_property_by_json(data, "map.test.1.color.1", "0.88");
        // test vec item
        Fundamental::io::set_property_by_json(data, "vec.color.1", "-2.0");
        // test object item
        vecTest colorTest;
        colorTest.color.emplace_back(-1.1f);
        colorTest.color.emplace_back(-1.2f);
        colorTest.color.emplace_back(-1.3f);
        colorTest.color.emplace_back(-1.4f);
        Fundamental::io::set_property_by_json(data, "map.test.2", Fundamental::io::to_json(colorTest));
        CombineData data2;
        Fundamental::io::from_json(str, data2);
        str = Fundamental::io::to_json(data2);
        std::cout << "CombineData test output:" << str << std::endl;
    }

    { // test register
        auto t = type::get_by_name("TestRegister");
        assert(t.is_valid());
    }
    return 0;
}
DECALRE_PLUGIN_INIT_FUNCTION(TestRegisterLib);
void TestPlugin()
{
#ifdef NDEBUG
    static string_view library_name("TestRegisterLib");
#else
    static string_view library_name("TestRegisterLib");
#endif
    library lib(library_name); // load the actual plugin

    if (!lib.load())
    {
        std::cerr << lib.get_error_string() << std::endl;
        return;
    }
    PROCESS_PLUGIN_INIT(TestRegisterLib);
    {
        for (auto t : lib.get_types())
        {
            std::cout << t.get_name() << std::endl;
        }
        // we cannot use the actual type, to get the type information,
        // thus we use string to retrieve it
        auto t = type::get_by_name("TestTypeRegister");

        // iterate over all methods of the class
        for (auto property : t.get_properties())
        {
            std::cout << property.get_name() << std::endl;
        }
    }
    {
        TestTypeRegister t;
        t.custom         = "custom";
        t.enumType       = TestEnumType::TestEnum4;
        t.name           = "test name";
        t.type           = "test type";
        std::string abcd = Fundamental::io::to_json(t);
        std::cout << "TestTypeRegister-test:" << abcd << std::endl;
        TestTypeRegister v2;
        Fundamental::io::from_json(abcd, v2);

        abcd = Fundamental::io::to_json(v2);
        std::cout << "TestTypeRegister-test:" << abcd << std::endl;
    }
    if (!lib.unload())
    {
        std::cerr << lib.get_error_string() << std::endl;
        return;
    }

    auto t1 = type::get_by_name("MyPlugin");
    if (t1.is_valid())
    {
        std::cerr << "the type: " << t1.get_name() << " should not be valid!";
    }
    {
        TestTypeRegister t;
        t.custom         = "custom";
        t.enumType       = TestEnumType::TestEnum4;
        t.name           = "test name";
        t.type           = "test type";
        std::string abcd = Fundamental::io::to_json(t);
        std::cout << "unload TestTypeRegister-test:" << abcd << std::endl;
        TestTypeRegister v2;
        Fundamental::io::from_json(abcd, v2);

        abcd = Fundamental::io::to_json(v2);
        std::cout << "unload TestTypeRegister-test:" << abcd << std::endl;
    }
    FWARN("exec member->{}",(void*)&TestInstance::x);
    FWARN("exec func->{}",(void*)TestInstance::GetInstance1());
    FWARN("exec external->{}",(void*)TestInstance::GetInstance2());
}