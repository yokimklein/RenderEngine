#pragma once
#include <type_traits>
#include <limits>

#define API_DIRECTX API_DX11 || API_DX12

#if API_DIRECTX
#include <DirectXMath.h>
using namespace DirectX;
#endif

constexpr bool K_SUCCESS = true;
constexpr bool K_FAILURE = false;

static_assert(sizeof(char) == 1);
static_assert(sizeof(short) == 2);

// An unsigned 8-bit integer ranged from 0 to 255
typedef unsigned char ubyte;
static_assert(sizeof(ubyte) == 1);

// A signed 8-bit integer ranged from -127 to 127
typedef char sbyte;
static_assert(sizeof(sbyte) == 1);

// An unsigned 16-bit integer ranged from 0 to 65535
typedef unsigned short uword;
static_assert(sizeof(uword) == 2);

// A signed 32-bit integer ranged from –2147483648 to 2147483648
typedef long int32;
static_assert(sizeof(int32) == 4);

// A signed 64-bit integer ranged from –9223372036854775808 to 9223372036854775807
typedef long long int64;
static_assert(sizeof(int64) == 8);

// An unsigned 32-bit integer ranged from 0 to 4294967295
typedef unsigned long dword;
static_assert(sizeof(dword) == 4);

// An unsigned 64-bit integer ranged from 0 to 18446744073709551615
typedef unsigned long long qword;
static_assert(sizeof(qword) == 8);

// int32
// int64
// dword
// qword
// short
// uword
// float

#ifdef PLATFORM_WINDOWS
constexpr dword MAXIMUM_PATH = 260;
#else
#warning NO MAXIMUM PATH SET FOR PLATFORM!
#endif

#define IN_RANGE_INCLUSIVE(value, min, max) ((value) >= (min) && (value) <= (max))
#define IN_RANGE_COUNT(value, min, count) ((value) >= (min) && (value) < (count))
	//#define IN_RANGE_INCLUSIVE(value, min, max) ((value) >= (min) && (value) <= (max))
#define IN_RANGE_BOUNDS(bounds, min, max) (bounds.values[0] >= min && bounds.values[1] <= max)

// this class provides an interface for setting bits via an enum with a specified storage size (t_storage_type)
template <typename t_flags, typename t_storage_type, t_flags k_maximum_count>
class c_flags
{
	static_assert(std::is_enum_v<t_flags>, "c_flags type must be an enumerator!");
	static_assert(std::numeric_limits<t_storage_type>::is_integer, "c_flags type must be an integer type!");
public:
	c_flags() : m_storage(0) {};
	c_flags(const t_flags value) : m_storage(static_cast<t_storage_type>(value)) {};
	c_flags(const t_storage_type value) : m_storage(value) {}

	template<typename T>
	inline bool operator==(const T value) const
	{
		return m_storage == static_cast<t_storage_type>(value);
	}
	c_flags<t_flags, t_storage_type, k_maximum_count> operator|(c_flags<t_flags, t_storage_type, k_maximum_count> const& other) const
	{
		return c_flags(m_storage | other.m_storage);
	}
	template<typename T> const
	inline bool operator!=(const T value)
	{
		return m_storage != static_cast<t_storage_type>(value);
	}
	template<typename T>
	inline bool operator<(const T value) const
	{
		return m_storage < static_cast<t_storage_type>(value);
	}
	template<typename T>
	inline bool operator>(const T value) const
	{
		return m_storage > static_cast<t_storage_type>(value);
	}
	template<typename T>
	inline bool operator>=(const T value) const
	{
		return m_storage >= static_cast<t_storage_type>(value);
	}
	template<typename T>
	inline bool operator<=(const T value) const
	{
		return m_storage <= static_cast<t_storage_type>(value);
	}
	template<typename T>
	inline void operator= (const T value)
	{
		m_storage = static_cast<t_storage_type>(value);
	}
	template <class T>
	operator T () const
	{
		return static_cast<T>(m_storage);
	}

	inline void set(t_flags bit, bool enable)
	{
		assert(bit < k_maximum_count);
		if (bit < k_maximum_count)
		{
			if (enable)
			{
				m_storage |= (1 << bit);
			}
			else
			{
				m_storage &= ~(1 << bit);
			}
		}
	}
	inline void clear()
	{
		m_storage = 0;
	}
	inline bool test(const t_flags bit) const
	{
		return (((m_storage) & (1 << (static_cast<t_storage_type>(bit)))) != 0);
	}
	t_storage_type const* get_bits_direct() const
	{
		return &m_storage;
	}
	t_storage_type* get_writeable_bits_direct()
	{
		return &m_storage;
	}

private:
	t_storage_type m_storage;
};

// Packs an enum to a specified storage size
template<typename t_enum, typename t_storage_type, t_enum k_enum_minimum, t_enum k_enum_count>
class c_packed_enum
{
	static_assert(std::is_enum_v<t_enum>, "c_packed_enum type must be an enumerator!");
	static_assert(std::numeric_limits<t_storage_type>::is_integer, "c_packed_enum type must be an integer type!");

public:
	// TODO: move method declarations to an .inl
	c_packed_enum() : m_storage(k_enum_minimum) {}
	c_packed_enum(const t_enum value) : m_storage(static_cast<t_storage_type>(value)) {}
	c_packed_enum(const t_storage_type value) : m_storage(value) {}

	template<typename T>
	bool operator==(const T value) const
	{
		assert(IN_RANGE_COUNT(value, k_enum_minimum, k_enum_count));
		return m_storage == static_cast<t_storage_type>(value);
	}
	template<typename T> const
	bool operator!=(const T value)
	{
		assert(IN_RANGE_COUNT(value, k_enum_minimum, k_enum_count));
		return m_storage != static_cast<t_storage_type>(value);
	}
	template<typename T>
	bool operator<(const T value) const
	{
		assert(IN_RANGE_COUNT(value, k_enum_minimum, k_enum_count));
		return m_storage < static_cast<t_storage_type>(value);
	}
	template<typename T>
	bool operator>(const T value) const
	{
		assert(IN_RANGE_COUNT(value, k_enum_minimum, k_enum_count));
		return m_storage > static_cast<t_storage_type>(value);
	}
	template<typename T>
	bool operator>=(const T value) const
	{
		assert(IN_RANGE_COUNT(value, k_enum_minimum, k_enum_count));
		return m_storage >= static_cast<t_storage_type>(value);
	}
	template<typename T>
	bool operator<=(const T value) const
	{
		assert(IN_RANGE_COUNT(value, k_enum_minimum, k_enum_count));
		return m_storage <= static_cast<t_storage_type>(value);
	}
	template<typename T>
	void operator=(const T value)
	{
		assert(IN_RANGE_COUNT(value, k_enum_minimum, k_enum_count));
		m_storage = static_cast<t_storage_type>(value);
	}

	template<typename T>
	void operator+=(const T value) = delete;
	template<typename T>
	void operator-=(const T value) = delete;
	template<typename T>
	void operator++() = delete;
	template<typename T>
	void operator--() = delete;

	template<class T>
	operator T() const
	{
		return static_cast<T>(m_storage);
	}

	t_enum get()
	{
		assert(IN_RANGE_COUNT(m_storage, k_enum_minimum, k_enum_count));
		return static_cast<t_enum>(m_storage);
	}
	t_enum const get() const
	{
		assert(IN_RANGE_COUNT(m_storage, k_enum_minimum, k_enum_count));
		return static_cast<t_enum>(m_storage);
	}

protected:
	t_storage_type m_storage;
};

struct point2d; // TODO: merge with vector2d
struct texcoord2d;
struct vector2d;
struct vector3d;
struct vector4d; // TODO: have quaternion inherit from this
struct colour_rgba;
struct matrix4x4;

struct point2d
{
	constexpr point2d(float x = 0.0f, float y = 0.0f)
	{
		this->x = x;
		this->y = y;
	}
	// vector3 conversion
	point2d(vector2d other);
	void operator=(vector2d other);
	point2d operator-(point2d other);
	point2d operator+(vector2d other);
	point2d operator+=(point2d other);
#if API_DIRECTX
	// DirectX Conversion
	point2d(XMFLOAT2 other);
	void operator=(const XMFLOAT2 other);
	XMFLOAT2 xmfloat();
#endif
	point2d rotate(float angle);

	union
	{
		struct
		{
			float x;
			float y;
		};
		float values[2];
	};
};

struct texcoord2d
{
	constexpr texcoord2d(float u = 0.0f, float v = 0.0f)
	{
		this->u = u;
		this->v = v;
	}
#if API_DIRECTX
	// DirectX Conversion
	texcoord2d(XMFLOAT2 other);
	void operator=(const XMFLOAT2 other);
	XMFLOAT2 xmfloat();
#endif

	union
	{
		struct
		{
			float u;
			float v;
		};
		float values[2];
	};
};

struct vector2d
{
	constexpr vector2d(float i = 0.0f, float j = 0.0f)
	{
		this->i = i;
		this->j = j;
	}
	// vector3 conversion
	vector2d(point2d other);
	void operator=(point2d other);
	vector2d operator-(vector2d other);
	vector2d operator+(point2d other);
	vector2d operator+=(vector2d other);
#if API_DIRECTX
	// DirectX Conversion
	vector2d(XMFLOAT2 other);
	void operator=(const XMFLOAT2 other);
	XMFLOAT2 xmfloat();
#endif

	union
	{
		struct
		{
			float i;
			float j;
		};
		float values[2];
	};
};

struct bounds2d
{
	constexpr bounds2d(float a = 0.0f, float b = 0.0f)
	{
		this->width = a;
		this->height = b;
	}
	union
	{
		struct
		{
			float width;
			float height;
		};
		struct
		{
			float min;
			float max;
		};
		float values[2];
	};
};

struct view_bounds2d
{
	constexpr view_bounds2d(dword a = 0, dword b = 0)
	{
		this->width = a;
		this->height = b;
	}
	union
	{
		struct
		{
			dword width;
			dword height;
		};
		dword values[2];
	};
};

struct vector3d
{
	constexpr vector3d(float i = 0.0f, float j = 0.0f, float k = 0.0f)
	{
		this->i = i;
		this->j = j;
		this->k = k;
	}
	//vector3d(vector3d& other);
	// vector3 conversion
	vector3d operator+(vector3d other);
	vector3d operator-(vector3d other);
	vector3d operator*(vector3d other);
	vector3d operator*(float other);
	vector3d operator/(float other);
	vector3d operator+=(vector3d other);
	vector3d operator+=(float other);
	vector3d operator*=(float other);
	const float operator[](long index) const;
#if API_DIRECTX
	// DirectX Conversion
	vector3d(XMFLOAT3 other);
	vector3d(XMVECTOR other);
	void operator=(const XMFLOAT3 other);
	XMFLOAT3 xmfloat();
	vector3d normalise();
	vector3d negate();
#endif
	float magnitude_square();
	float magnitude();
	vector3d square();

	union
	{
		struct
		{
			float x;
			float y;
			float z;
		};
		struct
		{
			float i;
			float j;
			float k;
		};
		float values[3];
	};
};

typedef vector3d point3d;

struct vector4d
{
	constexpr vector4d(float i = 0.0f, float j = 0.0f, float k = 0.0f, float w = 0.0f)
	{
		this->i = i;
		this->j = j;
		this->k = k;
		this->w = w;
	}
	// vector4 conversion
	vector4d(colour_rgba other);
	void operator=(colour_rgba other);
	void operator=(float other[4]);
	vector4d operator*(vector4d other);
#if API_DIRECTX
	// DirectX Conversion
	vector4d(XMFLOAT4 other);
	void operator=(const XMFLOAT4 other);
	XMFLOAT4 xmfloat();
#endif

	union
	{
		struct
		{
			float i;
			float j;
			float k;
			float w;
		};
		struct
		{
			vector3d v3;
			float w;
		};
		float values[4];
	};
};

// RGBA colours from 0.0f-1.0f
struct colour_rgba
{
	constexpr colour_rgba(float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f)
	{
		this->r = r;
		this->g = g;
		this->b = b;
		this->a = a;
	}
	// vector4 conversion
	colour_rgba(vector4d other);
	void operator=(vector4d other);
#if API_DIRECTX
	// DirectX Conversion
	colour_rgba(XMFLOAT4 other);
	void operator=(const XMFLOAT4 other);
	XMFLOAT4 xmfloat();
#endif

	union
	{
		struct
		{
			float r;
			float g;
			float b;
			float a;
		};
		float values[4];
	};
};

// imaginary number:
//  - can't be resolved mathematically
//  - i = sqrt(-1)
//  - i*i = -1
// complex number:
//  - combination of real and imaginary numbers
//  - allow you to resolve the imaginary part into a real number (-1) which you can perform calculations with
// complex conjugate:
//  - change the sign of the imaginary component
struct quaternion
{
	// Construct quaternion from ijkw components
	constexpr quaternion()
	{
		this->i = 0.0f;
		this->j = 0.0f;
		this->k = 0.0f;
		this->w = 1.0f;
	}
	constexpr quaternion(const float i, const float j, const float k, const float w)
	{
		this->i = i;
		this->j = j;
		this->k = k;
		this->w = w;
	}
	// Construct quaternion from Euler XYZ degrees
	quaternion(const float x, const float y, const float z);
	quaternion(vector3d vec) { *this = quaternion(vec.x, vec.y, vec.z); };

	float magnitude();
	float magnitude_squared();
	float angle();
	vector3d axis();
	quaternion rotate(quaternion q);
	vector3d rotate_vector(vector3d v);
	vector3d euler();

	quaternion operator+(quaternion q);
	quaternion operator+=(quaternion q);
	quaternion operator-=(quaternion q);
	quaternion operator*=(float s);
	quaternion operator/=(float s);
	quaternion operator~(void) const;

#if API_DIRECTX
	// DirectX Conversion
	void set_rotation_euler(vector3d rotation);
#endif

	union
	{
		struct
		{
			float i; // 'imaginary' unit
			float j; // 'imaginary' unit
			float k; // 'imaginary' unit
			float w; // the 'real' part of the quaternion
		};
		struct
		{
			vector3d qv; // 'imaginary'
			float qw; // 'real'
		};
		float values[4];
	};
};

//inline quaternion operator+(quaternion q1, quaternion q2);
inline quaternion operator-(quaternion q1, quaternion q2);
inline quaternion operator*(quaternion q1, quaternion q2);
inline quaternion operator*(quaternion q, float s);
inline quaternion operator*(float s, quaternion q);
inline quaternion operator*(quaternion q, vector3d v);
inline quaternion operator*(vector3d v, quaternion q);
inline quaternion operator/(quaternion q, float s);

point3d rotate_point_by_origin(point3d point, point3d origin, quaternion q);

struct matrix3x3
{
	constexpr matrix3x3()
	{
		v[0] = vector3d{ 1.0f, 0.0f, 0.0f };
		v[1] = vector3d{ 0.0f, 1.0f, 0.0f };
		v[2] = vector3d{ 0.0f, 0.0f, 1.0f };
	}
	void operator=(const matrix3x3 other);
	matrix3x3 operator*(const matrix3x3 other);
#if API_DIRECTX
	// DirectX Conversion
	matrix3x3(XMFLOAT3X3 other);
	void operator=(const XMFLOAT3X3 other);
	XMFLOAT3X3 xmfloat();
#endif

	union
	{
		float m[3][3];
		vector3d v[3];
	};
};

struct matrix4x4
{
	constexpr matrix4x4()
	{
		// Identity matrix
		v[0] = vector4d{ 1.0f, 0.0f, 0.0f, 0.0f };
		v[1] = vector4d{ 0.0f, 1.0f, 0.0f, 0.0f };
		v[2] = vector4d{ 0.0f, 0.0f, 1.0f, 0.0f };
		v[3] = vector4d{ 0.0f, 0.0f, 0.0f, 1.0f };
	}
	void operator=(const matrix4x4 other);
	matrix4x4 operator*(const matrix4x4 other);
#if API_DIRECTX
	// DirectX Conversion
	matrix4x4(XMFLOAT4X4 other);
	void operator=(const XMFLOAT4X4 other);
	XMFLOAT4X4 xmfloat();
#endif

	union
	{
		float m[4][4];
		vector4d v[4];
	};
};

vector3d direction_from_points(point3d from, point3d to);
#if API_DIRECTX
vector3d cross_product(vector3d a, vector3d b); // TODO: remove DX dependency on this
#endif
float dot_product(vector3d a, vector3d b);

constexpr float PI = 3.14159265358979323846f;
constexpr float MAX_RADIANS = 2 * PI;
constexpr float EPSILON_VALUE = 0.0001f; // can't call this EPSILON because its reserved
constexpr bounds2d DEGREES_RANGE = { -180.0f, 180.0f };
// uses degrees in range -180.0f to 180.0f
inline constexpr float deg2rad(float deg) { return deg * PI / DEGREES_RANGE.max; }
// converts degrees in range -180.0f to 180.0f
inline constexpr float rad2deg(float rad) { return rad * DEGREES_RANGE.max / PI; }

constexpr point3d WORLD_ORIGIN = { 0.0f, 0.0f, 0.0f };
constexpr vector3d GLOBAL_UP = { 0.0f, 1.0f, 0.0f };
constexpr point3d DEFAULT_SCALE = { 1.0f, 1.0f, 1.0f };
constexpr point3d DEFAULT_EULER_ROTATION = { 0.0f, 0.0f, 0.0f };
constexpr matrix4x4 IDENTITY_MATRIX = matrix4x4();

class c_transform
{
public:
	// Construct a transform with an XYZ position & scale and a quaternion rotation
	c_transform(point3d position = WORLD_ORIGIN, point3d scale = DEFAULT_SCALE, quaternion rotation = quaternion());
	// Construct a transform with an XYZ position & scale and an euler rotation
	c_transform(point3d position, point3d scale, point3d rotation);

	inline void set_position(point3d position) { m_position = position; };
	inline point3d get_position() { return m_position; };

#if API_DIRECTX
	inline void set_rotation_euler(point3d rotation) { m_rotation.set_rotation_euler(rotation); };
	inline point3d get_rotation_euler() { return m_rotation.euler(); };
#endif
	inline void set_rotation(quaternion rotation) { m_rotation = rotation; };
	inline quaternion get_rotation() { return m_rotation; };

	inline void set_scale(point3d scale) { m_scale = scale; };
	inline point3d get_scale() { return m_scale; };

	matrix4x4 build_matrix();

private:
	point3d m_position;
	quaternion m_rotation;
	point3d m_scale;
};

enum e_vertex_type
{
	_simple_vertex,
	_full_vertex,

	k_vertex_type_count
};

// used for post processing screen sized quad
// used as a triangle strip
struct simple_vertex
{
	vector4d position;
	point2d tex_coord;
};

// used in VBO loading and as part of a full vertex
// used as a triangle list w/ index buffer
struct vertex_part
{
	vector3d position;
	vector3d normal;
	point2d tex_coord;
};

// standard vertex type used in rendering
struct vertex
{
	vertex_part vertex;
	vector3d tangent;
	vector3d bitangent;
};