#ifndef COLORSPACE_HPP
#define COLORSPACE_HPP

#include "enums.hpp"
#define COLORSPACE_IS_FLAG 0
extern "C" {
#include <video/csputils.h>
}

enum class ColorSpace : int {
    Auto = (int)0,
    BT601 = (int)1,
    BT709 = (int)2,
    SMPTE240M = (int)3,
    YCgCo = (int)4,
    RGB = (int)5
};

Q_DECLARE_METATYPE(ColorSpace)

constexpr inline auto operator == (ColorSpace e, int i) -> bool { return (int)e == i; }
constexpr inline auto operator != (ColorSpace e, int i) -> bool { return (int)e != i; }
constexpr inline auto operator == (int i, ColorSpace e) -> bool { return (int)e == i; }
constexpr inline auto operator != (int i, ColorSpace e) -> bool { return (int)e != i; }
constexpr inline auto operator > (ColorSpace e, int i) -> bool { return (int)e > i; }
constexpr inline auto operator < (ColorSpace e, int i) -> bool { return (int)e < i; }
constexpr inline auto operator >= (ColorSpace e, int i) -> bool { return (int)e >= i; }
constexpr inline auto operator <= (ColorSpace e, int i) -> bool { return (int)e <= i; }
constexpr inline auto operator > (int i, ColorSpace e) -> bool { return i > (int)e; }
constexpr inline auto operator < (int i, ColorSpace e) -> bool { return i < (int)e; }
constexpr inline auto operator >= (int i, ColorSpace e) -> bool { return i >= (int)e; }
constexpr inline auto operator <= (int i, ColorSpace e) -> bool { return i <= (int)e; }
#if COLORSPACE_IS_FLAG
#include "enumflags.hpp"
using  = EnumFlags<ColorSpace>;
constexpr inline auto operator | (ColorSpace e1, ColorSpace e2) -> 
{ return (::IntType(e1) | ::IntType(e2)); }
constexpr inline auto operator ~ (ColorSpace e) -> EnumNot<ColorSpace>
{ return EnumNot<ColorSpace>(e); }
constexpr inline auto operator & (ColorSpace lhs,  rhs) -> EnumAnd<ColorSpace>
{ return rhs & lhs; }
Q_DECLARE_METATYPE()
#endif

template<>
class EnumInfo<ColorSpace> {
    typedef ColorSpace Enum;
public:
    typedef ColorSpace type;
    using Data =  mp_csp;
    struct Item {
        Enum value;
        QString name, key;
        mp_csp data;
    };
    using ItemList = std::array<Item, 6>;
    static constexpr auto size() -> int
    { return 6; }
    static constexpr auto typeName() -> const char*
    { return "ColorSpace"; }
    static constexpr auto typeKey() -> const char*
    { return "space"; }
    static auto typeDescription() -> QString
    { return qApp->translate("EnumInfo", "Color Space"); }
    static auto item(Enum e) -> const Item*
    { return 0 <= e && e < size() ? &info[(int)e] : nullptr; }
    static auto name(Enum e) -> QString
    { auto i = item(e); return i ? i->name : QString(); }
    static auto key(Enum e) -> QString
    { auto i = item(e); return i ? i->key : QString(); }
    static auto data(Enum e) -> mp_csp
    { auto i = item(e); return i ? i->data : mp_csp(); }
    static auto description(int e) -> QString
    { return description((Enum)e); }
    static auto description(Enum e) -> QString
    {
        switch (e) {
        case Enum::Auto: return qApp->translate("EnumInfo", "Auto");
        case Enum::BT601: return qApp->translate("EnumInfo", "BT.601(SD)");
        case Enum::BT709: return qApp->translate("EnumInfo", "BT.709(HD)");
        case Enum::SMPTE240M: return qApp->translate("EnumInfo", "SMPTE-240M");
        case Enum::YCgCo: return qApp->translate("EnumInfo", "YCgCo");
        case Enum::RGB: return qApp->translate("EnumInfo", "RGB");
        default: return QString();
        }
    }
    static constexpr auto items() -> const ItemList&
    { return info; }
    static auto from(int id, Enum def = default_()) -> Enum
    {
        auto it = std::find_if(info.cbegin(), info.cend(),
                               [id] (const Item &item)
                               { return item.value == id; });
        return it != info.cend() ? it->value : def;
    }
    static auto from(const QString &name, Enum def = default_()) -> Enum
    {
        auto it = std::find_if(info.cbegin(), info.cend(),
                               [&name] (const Item &item)
                               { return !name.compare(item.name); });
        return it != info.cend() ? it->value : def;
    }
    static auto fromName(Enum &val, const QString &name) -> bool
    {
        auto it = std::find_if(info.cbegin(), info.cend(),
                               [&name] (const Item &item)
                               { return !name.compare(item.name); });
        if (it == info.cend())
            return false;
        val = it->value;
        return true;
    }
    static auto fromData(const mp_csp &data,
                         Enum def = default_()) -> Enum
    {
        auto it = std::find_if(info.cbegin(), info.cend(),
                               [&data] (const Item &item)
                               { return item.data == data; });
        return it != info.cend() ? it->value : def;
    }
    static constexpr auto default_() -> Enum
    { return ColorSpace::Auto; }
private:
    static const ItemList info;
};

using ColorSpaceInfo = EnumInfo<ColorSpace>;

#endif
