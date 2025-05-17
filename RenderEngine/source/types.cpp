#include "types.h"
#include <corecrt_math.h>

quaternion::quaternion(const float x, const float y, const float z)
{
	const double roll = deg2rad(x);
	const double pitch = deg2rad(y);
	const double yaw = deg2rad(z);

	const double cyaw = cos(0.5f * yaw);
	const double cpitch = cos(0.5f * pitch);
	const double croll = cos(0.5f * roll);
	const double syaw = sin(0.5f * yaw);
	const double spitch = sin(0.5f * pitch);
	const double sroll = sin(0.5f * roll);

	const double cyawcpitch = cyaw * cpitch;
	const double syawspitch = syaw * spitch;
	const double cyawspitch = cyaw * spitch;
	const double syawcpitch = syaw * cpitch;

	this->i = (float)(cyawcpitch * sroll - syawspitch * croll);
	this->j = (float)(cyawspitch * croll + syawcpitch * sroll);
	this->k = (float)(syawcpitch * croll - cyawspitch * sroll);
	this->w = (float)(cyawcpitch * croll + syawspitch * sroll);
}
float quaternion::magnitude()
{
	return (float)sqrt(w * w + i * i + j * j + k * k);
}
float quaternion::magnitude_squared()
{
	return w * w + i * i + j * j + k * k;
}
float quaternion::angle()
{
	return (float)(2 * acos(this->w));
}
vector3d quaternion::axis()
{
	vector3d v = this->qv;
	float m = v.magnitude();

	if (m <= 0.0f)
		return vector3d();
	else
		return v / m;
}
quaternion quaternion::rotate(quaternion q)
{
	return *this * q * (~*this);
}
vector3d quaternion::rotate_vector(vector3d v)
{
	quaternion t = *this * v * (~*this);
	return t.qv;
}
vector3d quaternion::euler()
{
#if API_DIRECTX
	matrix4x4 mat;
	XMStoreFloat4x4((XMFLOAT4X4*)&mat, XMMatrixRotationQuaternion(XMLoadFloat4((XMFLOAT4*)values)));

	vector3d rot_euler;
	rot_euler.i = rad2deg(atan2f(mat.m[1][2], mat.m[2][2]));
	rot_euler.j = rad2deg(atan2f(-mat.m[0][2], sqrtf(mat.m[1][2] * mat.m[1][2] + mat.m[2][2] * mat.m[2][2])));
	rot_euler.k = rad2deg(atan2f(mat.m[0][1], mat.m[0][0]));

	return rot_euler;
#else
#error TODO: need a non DX way of doing this
	// TODO: THIS HAS PRECISION ISSUES!!!
	double q11 = this->i * this->i;
	double q22 = this->j * this->j;
	double q33 = this->k * this->k;
	double q00 = this->w * this->w;

	double r11 = q00 + q11 - q22 - q33;
	double r21 = 2 * (this->i * this->j + this->w * this->k);
	double r31 = 2 * (this->i * this->k - this->w * this->j);
	double r32 = 2 * (this->j * this->k + this->w * this->i);
	double r33 = q00 - q11 - q22 + q33;

	vector3d u;
	double tmp = fabs(r31);
	if (tmp > 0.999999)
	{
		double r12 = 2 * (this->i * this->j - this->w * this->k);
		double r13 = 2 * (this->i * this->k + this->w * this->j);

		u.x = rad2deg(0.0f); // roll
		u.y = rad2deg((float)(-(PI / 2) * r31 / tmp)); // pitch
		u.z = rad2deg((float)atan2(-r12, -r31 * r13)); // yaw
		return u;
	}

	u.x = rad2deg((float)atan2(r32, r33)); // roll
	u.y = rad2deg((float)asin(-r31));      // pitch
	u.z = rad2deg((float)atan2(r21, r11)); // yaw
	return u;
#endif
}
quaternion quaternion::operator+(quaternion q)
{
	quaternion result = *this;
	result.i += q.i;
	result.j += q.j;
	result.k += q.k;
	result.w += q.w;
	return result;
}
quaternion quaternion::operator+=(quaternion q)
{
	this->i += q.i;
	this->j += q.j;
	this->k += q.k;
	this->w += q.w;
	return *this;
}
quaternion quaternion::operator-=(quaternion q)
{
	this->i -= q.i;
	this->j -= q.j;
	this->k -= q.k;
	this->w -= q.w;
	return *this;
}
quaternion quaternion::operator*=(float s)
{
	this->i *= s;
	this->j *= s;
	this->k *= s;
	this->w *= s;
	return *this;
}
quaternion quaternion::operator/=(float s)
{
	this->i /= s;
	this->j /= s;
	this->k /= s;
	this->w /= s;
	return *this;
}
quaternion quaternion::operator~(void) const
{
	return quaternion(-i, -j, -k, w);
}
//inline quaternion operator+(quaternion q1, quaternion q2)
//{
//	return quaternion(q1.i + q2.i, q1.j + q2.j, q1.k + q2.k, q1.w + q2.w);
//}
inline quaternion operator-(quaternion q1, quaternion q2)
{
	return quaternion(q1.i - q2.i, q1.j - q2.j, q1.k - q2.k, q1.w - q2.w);
}
inline quaternion operator*(quaternion q1, quaternion q2)
{
	return quaternion
	(
		q1.w * q2.i + q1.i * q2.w +
		q1.j * q2.k - q1.k * q2.j,
		q1.w * q2.j + q1.j * q2.w +
		q1.k * q2.i - q1.i * q2.k,
		q1.w * q2.k + q1.k * q2.w +
		q1.i * q2.j - q1.j * q2.i,
		q1.w * q2.w - q1.i * q2.i -
		q1.j * q2.j - q1.k * q2.k
	);
}
inline quaternion operator*(quaternion q, float s)
{
	return quaternion(q.i * s, q.j * s, q.k * s, q.w * s);
}
inline quaternion operator*(float s, quaternion q)
{
	return quaternion(q.i * s, q.j * s, q.k * s, q.w * s);
}
inline quaternion operator*(quaternion q, vector3d v)
{
	return quaternion(
		q.w * v.x + q.j * v.z - q.k * v.y,
		q.w * v.y + q.k * v.x - q.i * v.z,
		q.w * v.z + q.i * v.y - q.j * v.x,
		-(q.i * v.x + q.j * v.y + q.k * v.z));
}
inline quaternion operator*(vector3d v, quaternion q)
{
	return quaternion(
		q.w * v.x + q.j * v.z - q.k * v.y,
		q.w * v.y + q.k * v.x - q.i * v.z,
		q.w * v.z + q.i * v.y - q.j * v.x,
		-(q.i * v.x + q.j * v.y + q.k * v.z));
}
inline quaternion operator/(quaternion q, float s)
{
	return quaternion(q.i / s, q.j / s, q.k / s, q.w / s);
}

#if API_DIRECTX
point3d rotate_point_by_origin(point3d point, point3d origin, quaternion q)
{
	point3d local_point = point - origin;
	XMVECTOR rotated_vector = XMVector3Rotate(XMLoadFloat3((XMFLOAT3*)&local_point), XMLoadFloat4((XMFLOAT4*)&q));

	point3d local_rotated_point;
	XMStoreFloat3((XMFLOAT3*)&local_rotated_point, rotated_vector);

	point3d rotated_point = origin + local_rotated_point;
	return rotated_point;
}

// DirectX conversion
texcoord2d::texcoord2d(XMFLOAT2 other)
{
	*this = other;
}
void texcoord2d::operator=(const XMFLOAT2 other)
{
	this->u = other.x;
	this->v = other.y;
}
XMFLOAT2 texcoord2d::xmfloat()
{
	return XMFLOAT2(u, v);
}
vector2d::vector2d(XMFLOAT2 other)
{
	*this = other;
}
void vector2d::operator=(const XMFLOAT2 other)
{
	this->i = other.x;
	this->j = other.y;
}
XMFLOAT2 vector2d::xmfloat()
{
	return XMFLOAT2(i, j);
}
point2d::point2d(XMFLOAT2 other)
{
	*this = other;
}
void point2d::operator=(const XMFLOAT2 other)
{
	this->x = other.x;
	this->y = other.y;
}
XMFLOAT2 point2d::xmfloat()
{
	return XMFLOAT2(x, y);
}
#endif

point2d point2d::rotate(float angle)
{
	point2d out_point = *this;
	float sin_angle = sinf(angle);
	float cos_angle = cosf(angle);
	out_point.x = (cos_angle * this->x) - (sin_angle * this->y);
	out_point.y = (sin_angle * this->x) + (cos_angle * this->y);
	return out_point;
}

#if API_DIRECTX
vector3d::vector3d(XMFLOAT3 other)
{
	*this = other;
}
vector3d::vector3d(XMVECTOR other)
{
	this->values[0] = other.m128_f32[0];
	this->values[1] = other.m128_f32[1];
	this->values[2] = other.m128_f32[2];
}
void vector3d::operator=(const XMFLOAT3 other)
{
	this->i = other.x;
	this->j = other.y;
	this->k = other.z;
}
XMFLOAT3 vector3d::xmfloat()
{
	return XMFLOAT3(i, j, k);
}
vector3d vector3d::normalise()
{
	return XMVector3Normalize(XMLoadFloat3((XMFLOAT3*)this));
}
vector3d vector3d::negate()
{
	return XMVectorNegate(XMLoadFloat3((XMFLOAT3*)this));
}
#endif

float vector3d::magnitude_square()
{
	return (this->i * this->i) + (this->j * this->j) + (this->k * this->k);
}
float vector3d::magnitude()
{
	return (float)sqrt(this->magnitude_square());
}
vector3d vector3d::square()
{
	vector3d vec = *this;
	vec.x *= vec.x;
	vec.y *= vec.y;
	vec.z *= vec.z;
	return vec;
}

#if API_DIRECTX
vector4d::vector4d(XMFLOAT4 other)
{
	*this = other;
}
void vector4d::operator=(const XMFLOAT4 other)
{
	this->i = other.x;
	this->j = other.y;
	this->k = other.z;
	this->w = other.w;
}
XMFLOAT4 vector4d::xmfloat()
{
	return XMFLOAT4(i, j, k, w);
}
colour_rgba::colour_rgba(XMFLOAT4 other)
{
	*this = other;
}
void colour_rgba::operator=(const XMFLOAT4 other)
{
	this->r = other.x;
	this->g = other.y;
	this->b = other.z;
	this->a = other.w;
}
XMFLOAT4 colour_rgba::xmfloat()
{
	return XMFLOAT4(r, g, b, a);
}

matrix3x3::matrix3x3(XMFLOAT3X3 other)
{
	*this = other;
}
void matrix3x3::operator=(const XMFLOAT3X3 other)
{
	m[0][0] = other.m[0][0]; m[0][1] = other.m[0][1]; m[0][2] = other.m[0][2];
	m[1][0] = other.m[1][0]; m[1][1] = other.m[1][1]; m[1][2] = other.m[1][2];
	m[2][0] = other.m[2][0]; m[2][1] = other.m[2][1]; m[2][2] = other.m[2][2];
}
XMFLOAT3X3 matrix3x3::xmfloat()
{
	return XMFLOAT3X3
	(
		m[0][0], m[0][1], m[0][2],
		m[1][0], m[1][1], m[1][2],
		m[2][0], m[2][1], m[2][2]
	);
}

matrix4x4::matrix4x4(XMFLOAT4X4 other)
{
	*this = other;
}
void matrix4x4::operator=(const XMFLOAT4X4 other)
{
	m[0][0] = other.m[0][0]; m[0][1] = other.m[0][1]; m[0][2] = other.m[0][2]; m[0][3] = other.m[0][3];
	m[1][0] = other.m[1][0]; m[1][1] = other.m[1][1]; m[1][2] = other.m[1][2]; m[1][3] = other.m[1][3];
	m[2][0] = other.m[2][0]; m[2][1] = other.m[2][1]; m[2][2] = other.m[2][2]; m[2][3] = other.m[2][3];
	m[3][0] = other.m[3][0]; m[3][1] = other.m[3][1]; m[3][2] = other.m[3][2]; m[3][3] = other.m[3][3];
}
XMFLOAT4X4 matrix4x4::xmfloat()
{
	return XMFLOAT4X4
	(
		m[0][0], m[0][1], m[0][2], m[0][3],
		m[1][0], m[1][1], m[1][2], m[1][3],
		m[2][0], m[2][1], m[2][2], m[2][3],
		m[3][0], m[3][1], m[3][2], m[3][3]
	);
}
void quaternion::set_rotation_euler(vector3d rotation)
{
	// convert degrees to radians
	rotation.x = deg2rad(rotation.x);
	rotation.y = deg2rad(rotation.y);
	rotation.z = deg2rad(rotation.z);

	// convert radians to quaternion
	XMVECTOR rotation_quat = XMQuaternionRotationRollPitchYaw(rotation.x, rotation.y, rotation.z);

	this->i = rotation_quat.m128_f32[0];
	this->j = rotation_quat.m128_f32[1];
	this->k = rotation_quat.m128_f32[2];
	this->w = rotation_quat.m128_f32[3];
}
#endif

// real conversion
point2d::point2d(vector2d other)
{
	*this = other;
}
void point2d::operator=(vector2d other)
{
	this->x = other.i;
	this->y = other.j;
}
point2d point2d::operator-(point2d other)
{
	point2d result;
	result.x = this->x - other.x;
	result.y = this->y - other.y;
	return result;
}
point2d point2d::operator+(vector2d other)
{
	point2d result;
	result.x = this->x + other.i;
	result.y = this->y + other.j;
	return result;
}
point2d point2d::operator+=(point2d other)
{
	this->x += other.x;
	this->y += other.y;
	return *this;
}
vector2d::vector2d(point2d other)
{
	*this = other;
}
void vector2d::operator=(point2d other)
{
	this->i = other.x;
	this->j = other.y;
}
vector2d vector2d::operator-(vector2d other)
{
	vector2d result;
	result.i = this->i - other.i;
	result.j = this->j - other.j;
	return result;
}
vector2d vector2d::operator+(point2d other)
{
	vector2d result;
	result.i = this->i + other.x;
	result.j = this->j + other.y;
	return result;
}
vector2d vector2d::operator+=(vector2d other)
{
	this->i += other.i;
	this->j += other.j;
	return *this;
}
vector3d vector3d::operator+=(vector3d other)
{
	this->i += other.i;
	this->j += other.j;
	this->k += other.k;
	return *this;
}
vector3d vector3d::operator+=(float other)
{
	this->i += other;
	this->j += other;
	this->k += other;
	return *this;
}
vector3d vector3d::operator*=(float other)
{
	this->i *= other;
	this->j *= other;
	this->k *= other;
	return *this;
}
vector3d vector3d::operator+(vector3d other)
{
	vector3d result;
	result.i = this->i + other.i;
	result.j = this->j + other.j;
	result.k = this->k + other.k;
	return result;
}
vector3d vector3d::operator-(vector3d other)
{
	vector3d result;
	result.i = this->i - other.i;
	result.j = this->j - other.j;
	result.k = this->k - other.k;
	return result;
}
vector3d vector3d::operator*(vector3d other)
{
	vector3d result;
	result.i = this->i * other.i;
	result.j = this->j * other.j;
	result.k = this->k * other.k;
	return result;
}
vector3d vector3d::operator*(float other)
{
	vector3d result;
	result.i = this->i * other;
	result.j = this->j * other;
	result.k = this->k * other;
	return result;
}
vector3d vector3d::operator/(float other)
{
	vector3d result;
	result.i = this->i / other;
	result.j = this->j / other;
	result.k = this->k / other;
	return result;
}
const float vector3d::operator[](long index) const
{
	if (index == 0) return i;
	if (index == 1) return j;
	if (index == 2) return k;

	//LOG_ERROR("Index out of bounds!");
	return 0;
}
vector4d::vector4d(colour_rgba other)
{
	*this = other;
}
void vector4d::operator=(colour_rgba other)
{
	this->i = other.r;
	this->j = other.g;
	this->k = other.b;
	this->w = other.a;
}
void vector4d::operator=(float other[4])
{
	this->i = other[0];
	this->j = other[1];
	this->k = other[2];
	this->w = other[3];
}
vector4d vector4d::operator*(vector4d other)
{
	vector4d vector = *this;
	vector.i *= other.i;
	vector.j *= other.j;
	vector.k *= other.k;
	vector.w *= other.w;
	return vector;
}
colour_rgba::colour_rgba(vector4d other)
{
	*this = other;
}
void colour_rgba::operator=(vector4d other)
{
	this->r = other.i;
	this->g = other.j;
	this->b = other.k;
	this->a = other.w;
}

void matrix3x3::operator=(const matrix3x3 other)
{
	this->v[0] = other.v[0];
	this->v[1] = other.v[1];
	this->v[2] = other.v[2];
}
matrix3x3 matrix3x3::operator*(const matrix3x3 other)
{
	matrix3x3 matrix = *this;
	matrix.v[0] = matrix.v[0] * other.v[0];
	matrix.v[1] = matrix.v[1] * other.v[1];
	matrix.v[2] = matrix.v[2] * other.v[2];
	return matrix;
}

void matrix4x4::operator=(const matrix4x4 other)
{
	this->v[0] = other.v[0];
	this->v[1] = other.v[1];
	this->v[2] = other.v[2];
	this->v[3] = other.v[3];
}

matrix4x4 matrix4x4::operator*(const matrix4x4 other)
{
	matrix4x4 matrix = *this;
	matrix.v[0] = matrix.v[0] * other.v[0];
	matrix.v[1] = matrix.v[1] * other.v[1];
	matrix.v[2] = matrix.v[2] * other.v[2];
	matrix.v[3] = matrix.v[3] * other.v[3];
	return matrix;
}

#if API_DIRECTX
vector3d direction_from_points(point3d from, point3d to)
{
	vector3d direction = to - from;
	XMVECTOR normalized = XMVector3Normalize(XMLoadFloat3((XMFLOAT3*)&direction));
	direction.values[0] = normalized.m128_f32[0]; direction.values[1] = normalized.m128_f32[1]; direction.values[2] = normalized.m128_f32[2];
	return direction.normalise();
}

vector3d cross_product(vector3d a, vector3d b)
{
	XMVECTOR vec = XMVector3Cross(XMLoadFloat3((XMFLOAT3*)&a), XMLoadFloat3((XMFLOAT3*)&b));
	vector3d result;
	XMStoreFloat3((XMFLOAT3*)&result, vec);
	return result;
}
#endif

float dot_product(vector3d a, vector3d b)
{
	//XMVECTOR vector = XMVector3Dot(XMLoadFloat3((XMFLOAT3*)&a), XMLoadFloat3((XMFLOAT3*)&b));
	//float result;
	//XMStoreFloat(&result, vector);
	//return result;
	return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

c_transform::c_transform(point3d position, point3d scale, quaternion rotation)
	: m_position(position)
	, m_scale(scale)
	, m_rotation(rotation)
{
}

c_transform::c_transform(point3d position, point3d scale, point3d rotation)
	: m_position(position)
	, m_scale(scale)
{
	m_rotation.set_rotation_euler(rotation);
}

matrix4x4 c_transform::build_matrix()
{
	matrix4x4 matrix = matrix4x4();

	// TODO: build different matrices for different graphics APIs. This is currently for DirectX only
#if API_DIRECTX
	// For DirectX (lefthanded, +X +Y +Z, right up forward)
	/*
	matrix.m[3][3] = 1.0f;

	matrix.m[0][0] = m_scale.x;
	matrix.m[1][1] = m_scale.y;
	matrix.m[2][2] = m_scale.z;

	matrix.m[3][0] = m_position.x;
	matrix.m[3][1] = m_position.y;
	matrix.m[3][2] = m_position.z;


	// calculate the rotation matrix from the quaternion
	// which components of the rotation matrix are forward up right?

	matrix4x4 test_rotation_matrix = matrix4x4();
	test_rotation_matrix.m[3][3] = 1.0f;

	test_rotation_matrix.m[0][0] = 1 - (2 * ((m_rotation.j * m_rotation.j) + (m_rotation.k * m_rotation.k)));
	test_rotation_matrix.m[0][1] = 2 * ((m_rotation.i * m_rotation.j) - (m_rotation.w * m_rotation.k));
	test_rotation_matrix.m[0][2] = 2 * ((m_rotation.i * m_rotation.k) + (m_rotation.w * m_rotation.j));

	test_rotation_matrix.m[1][0] = 2 * ((m_rotation.i * m_rotation.j) + (m_rotation.w * m_rotation.k));
	test_rotation_matrix.m[1][1] = 1 - 2 * ((m_rotation.i * m_rotation.i) + (m_rotation.k * m_rotation.k));
	test_rotation_matrix.m[1][2] = 2 * ((m_rotation.j * m_rotation.k) - (m_rotation.w * m_rotation.i));

	test_rotation_matrix.m[2][0] = 2 * ((m_rotation.i * m_rotation.k) - (m_rotation.w * m_rotation.j));
	test_rotation_matrix.m[2][1] = 2 * ((m_rotation.j * m_rotation.k) + (m_rotation.w * m_rotation.i));
	test_rotation_matrix.m[2][2] = 1 - 2 * ((m_rotation.i * m_rotation.i) + (m_rotation.j * m_rotation.j)); // OKAY

	matrix4x4 test_rotation_matrix2 = matrix4x4();
	test_rotation_matrix2 = matrix * test_rotation_matrix;
	//matrix r00 = 2 * (q0 * q0 + q1 * q1) - 1;
	//r01 = 2 * (q1 * q2 - q0 * q3);
	//r02 = 2 * (q1 * q3 + q0 * q2);

	// 01 02 03
	// 10 12 13
	// 20 21 23
	*/
	// TEMP SETUP
	XMMATRIX translation_matrix = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
	XMMATRIX rotation_matrix = XMMatrixRotationQuaternion(XMLoadFloat4((XMFLOAT4*)&m_rotation));
	XMMATRIX scale_matrix = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
	XMMATRIX final_matrix = scale_matrix * rotation_matrix * translation_matrix;
	return *(matrix4x4*)&final_matrix;
#else
#warning MATRIX BUILD MATH NOT SUPPORTED ON THIS API, THIS WILL ONLY EVER RETURN THE IDENTITY MATRIX!
	return matrix;
#endif
}