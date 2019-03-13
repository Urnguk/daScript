#pragma once

#include "daScript/misc/string_writer.h"

namespace das
{
    template <typename TT>
    struct vec2 {
        TT   x, y;
        template <typename AP>
        __forceinline friend StringWriter<AP> & operator<< (StringWriter<AP> & stream, const vec2<TT> & vec) {
            stream << vec.x << "," << vec.y;
            return stream;
        }
        __forceinline bool operator == ( const vec2<TT> & vec ) const {
            return x==vec.x && y==vec.y;
        }
    };

    template <typename TT>
    struct vec3 {
        TT   x, y, z;
        template <typename AP>
        __forceinline friend StringWriter<AP> & operator<< (StringWriter<AP> & stream, const vec3<TT> & vec) {
            stream << vec.x << "," << vec.y << "," << vec.z;
            return stream;
        }
        __forceinline bool operator == ( const vec3<TT> & vec ) const {
            return x==vec.x && y==vec.y && z==vec.z;
        }
        vec3() = default;
        vec3(const vec3 &) = default;
        vec3(TT X, TT Y, TT Z) : x(X), y(Y), z(Z) {}
        vec3(TT t) : x(t), y(t), z(t) {}
        operator vec4f() const { return v_ldu((float *)this); }
    };

    template <typename TT>
    struct vec4 {
        TT  x, y, z, w;
        template <typename AP>
        __forceinline friend StringWriter<AP> & operator<< (StringWriter<AP> & stream, const vec4<TT> & vec) {
            stream << vec.x << "," << vec.y << "," << vec.z << "," << vec.w;
            return stream;
        }
        __forceinline bool operator == ( const vec4<TT> & vec ) const {
            return x==vec.x && y==vec.y && z==vec.z && w==vec.w;
        }
    };

    typedef vec2<float> float2;
    typedef vec3<float> float3;
    typedef vec4<float> float4;

    typedef vec2<int32_t> int2;
    typedef vec3<int32_t> int3;
    typedef vec4<int32_t> int4;

    typedef vec2<uint32_t> uint2;
    typedef vec3<uint32_t> uint3;
    typedef vec4<uint32_t> uint4;
}

