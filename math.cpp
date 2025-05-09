#include "includes.h"

void math::AngleMatrix( const ang_t& ang, const vec3_t& pos, matrix3x4_t& out ) {
    g_csgo.AngleMatrix( ang, out );
    out.SetOrigin( pos );
}

vec3_t math::vector_angles(const vec3_t& v) {
    float magnitude = sqrtf(v.x * v.x + v.y * v.y);
    float pitch = atan2f(-v.z, magnitude) * M_RADPI;
    float yaw = atan2f(v.y, v.x) * M_RADPI;

    vec3_t angle(pitch, yaw, 0.0f);
    return angle;
}

vec3_t math::angle_vectors(const vec3_t& angles) {
    float sp, sy, cp, cy;
    sp = sinf(angles.x * M_PIRAD);
    cp = cosf(angles.x * M_PIRAD);
    sy = sinf(angles.y * M_PIRAD);
    cy = cosf(angles.y * M_PIRAD);

    return vec3_t{ cp * cy, cp * sy, -sp };
}


void math::NormalizeAngle( float &angle ) {
    float rot;

    // bad number.
    if( !std::isfinite( angle ) ) {
        angle = 0.f;
        return;
    }

    // no need to normalize this angle.
    if( angle >= -180.f && angle <= 180.f )
        return;

    // get amount of rotations needed.
    rot = std::round( std::abs( angle / 360.f ) );

    // normalize.
    angle = ( angle < 0.f ) ? angle + ( 360.f * rot ) : angle - ( 360.f * rot );
}

float math::SmoothStepBounds(float edge0, float edge1, float x) {
    float v1 = std::clamp((x - edge0) / (edge1 - edge0), 0.f, 1.f);
    return v1 * v1 * (3 - 2 * v1);
}

float math::ClampCycle(float flCycleIn) {
    flCycleIn -= int(flCycleIn);

    if (flCycleIn < 0) {
        flCycleIn += 1;
    }
    else if (flCycleIn > 1) {
        flCycleIn -= 1;
    }

    return flCycleIn;
}

float math::ApproachAngle( float target, float value, float speed ) {
    float delta;

    target = AngleMod( target );
    value  = AngleMod( value );
    delta  = target - value;

    // speed is assumed to be positive.
    speed = std::abs( speed );

    math::NormalizeAngle( delta );

    if( delta > speed )
        value += speed;

    else if( delta < -speed )
        value -= speed;

    else
        value = target;

    return value;
}

float math::SegmentToSegment(const vec3_t s1, const vec3_t s2, const vec3_t k1, const vec3_t k2)
{
    static auto constexpr epsilon = 0.00000001;

    auto u = s2 - s1;
    auto v = k2 - k1;
    const auto w = s1 - k1;

    const auto a = u.dot(u);
    const auto b = u.dot(v);
    const auto c = v.dot(v);
    const auto d = u.dot(w);
    const auto e = v.dot(w);
    const auto D = a * c - b * b;
    float sn, sd = D;
    float tn, td = D;

    if (D < epsilon) {
        sn = 0.0;
        sd = 1.0;
        tn = e;
        td = c;
    }
    else {
        sn = b * e - c * d;
        tn = a * e - b * d;

        if (sn < 0.0) {
            sn = 0.0;
            tn = e;
            td = c;
        }
        else if (sn > sd) {
            sn = sd;
            tn = e + b;
            td = c;
        }
    }

    if (tn < 0.0) {
        tn = 0.0;

        if (-d < 0.0)
            sn = 0.0;
        else if (-d > a)
            sn = sd;
        else {
            sn = -d;
            sd = a;
        }
    }
    else if (tn > td) {
        tn = td;

        if (-d + b < 0.0)
            sn = 0;
        else if (-d + b > a)
            sn = sd;
        else {
            sn = -d + b;
            sd = a;
        }
    }

    const float sc = abs(sn) < epsilon ? 0.0 : sn / sd;
    const float tc = abs(tn) < epsilon ? 0.0 : tn / td;

    m128 n;
    auto dp = w + u * sc - v * tc;
    n.f[0] = dp.dot(dp);
    const auto calc = sqrt_ps(n.v);
    return reinterpret_cast<const m128*>(&calc)->f[0];

}

void math::VectorAngles( const vec3_t& forward, ang_t& angles, vec3_t *up ) {
    vec3_t  left;
    float   len, up_z, pitch, yaw, roll;

    // get 2d length.
    len = forward.length_2d( );

    if( up && len > 0.001f ) {
        pitch = rad_to_deg( std::atan2( -forward.z, len ) );
        yaw   = rad_to_deg( std::atan2( forward.y, forward.x ) );

        // get left direction vector using cross product.
        left = ( *up ).cross( forward ).normalized( );

        // calculate up_z.
        up_z = ( left.y * forward.x ) - ( left.x * forward.y );

        // calculate roll.
        roll = rad_to_deg( std::atan2( left.z, up_z ) );
    }

    else {
        if( len > 0.f ) {
            // calculate pitch and yaw.
            pitch = rad_to_deg( std::atan2( -forward.z, len ) );
            yaw   = rad_to_deg( std::atan2( forward.y, forward.x ) );
            roll  = 0.f;
        }

        else {
            pitch = ( forward.z > 0.f ) ? -90.f : 90.f;
            yaw   = 0.f;
            roll  = 0.f;
        }
    }

    // set out angles.
    angles = { pitch, yaw, roll };
}

void inline math::SinCos(float radians, float* sine, float* cosine)
{
    _asm
    {
        fld		DWORD PTR[radians]
        fsincos

        mov edx, DWORD PTR[cosine]
        mov eax, DWORD PTR[sine]

        fstp DWORD PTR[edx]
        fstp DWORD PTR[eax]
    }
}

void math::matrix_set_column(const vec3_t& in, int column, matrix3x4_t& out)
{
    out[0][column] = in.x;
    out[1][column] = in.y;
    out[2][column] = in.z;
}

vec3_t math::vector_rotate(const vec3_t& in1, const matrix3x4_t& in2)
{
    return vec3_t(in1.Dot(in2[0]), in1.Dot(in2[1]), in1.Dot(in2[2]));
}

void math::angle_matrix(const ang_t& angles, const vec3_t& position, matrix3x4_t& matrix)
{
    angle_matrix(angles, matrix);
    matrix_set_column(position, 3, matrix);
}

void math::angle_matrix(const ang_t& angles, matrix3x4_t& matrix)
{
    float sr, sp, sy, cr, cp, cy;

    SinCos(DEG2RAD(angles.x), &sp, &cp);
    SinCos(DEG2RAD(angles.y), &sy, &cy);
    SinCos(DEG2RAD(angles.z), &sr, &cr);

    // matrix = (YAW * PITCH) * ROLL
    matrix[0][0] = cp * cy;
    matrix[1][0] = cp * sy;
    matrix[2][0] = -sp;

    const auto crcy = cr * cy;
    const auto crsy = cr * sy;
    const auto srcy = sr * cy;
    const auto srsy = sr * sy;
    matrix[0][1] = sp * srcy - crsy;
    matrix[1][1] = sp * srsy + crcy;
    matrix[2][1] = sr * cp;

    matrix[0][2] = (sp * crcy + srsy);
    matrix[1][2] = (sp * crsy - srcy);
    matrix[2][2] = cr * cp;

    matrix[0][3] = 0.0f;
    matrix[1][3] = 0.0f;
    matrix[2][3] = 0.0f;
}

void math::AngleVectors(const ang_t& angles, vec3_t& forward) {
    const auto sp = sin(deg_to_rad(angles.x)), cp = cos(deg_to_rad(angles.x)),
        sy = sin(deg_to_rad(angles.y)), cy = cos(deg_to_rad(angles.y));

    forward.x = cp * cy;
    forward.y = cp * sy;
    forward.z = -sp;
}

void math::AngleVectors(const ang_t& angles, vec3_t* forward, vec3_t* right, vec3_t* up) {
    float cp = std::cos(deg_to_rad(angles.x)), sp = std::sin(deg_to_rad(angles.x));
    float cy = std::cos(deg_to_rad(angles.y)), sy = std::sin(deg_to_rad(angles.y));
    float cr = std::cos(deg_to_rad(angles.z)), sr = std::sin(deg_to_rad(angles.z));

    if (forward) {
        forward->x = cp * cy;
        forward->y = cp * sy;
        forward->z = -sp;
    }

    if (right) {
        right->x = -1.f * sr * sp * cy + -1.f * cr * -sy;
        right->y = -1.f * sr * sp * sy + -1.f * cr * cy;
        right->z = -1.f * sr * cp;
    }

    if (up) {
        up->x = cr * sp * cy + -sr * -sy;
        up->y = cr * sp * sy + -sr * cy;
        up->z = cr * cp;
    }
}

void math::AngleVectorKidua(ang_t& vAngle, vec3_t& vForward)
{
    float sp, sy, cp, cy;
    SinCos(vAngle[1] * math::pi / 180.f, &sy, &cy);
    SinCos(vAngle[0] * math::pi / 180.f, &sp, &cp);

    vForward[0] = cp * cy;
    vForward[1] = cp * sy;
    vForward[2] = -sp;
}

ang_t math::look(const vec3_t& from, const vec3_t& target)
{
    vec3_t delta = target - from;
    ang_t angles;

    if (delta.y == 0.0f && delta.x == 0.0f)
    {
        angles.x = (delta.z > 0.0f) ? 270.0f : 90.0f;
        angles.y = 0.0f;
    }
    else
    {
        angles.x = static_cast<float>(-atan2(-delta.z, delta.length_2d()) * -180.f / M_PI);
        angles.y = static_cast<float>(atan2(delta.y, delta.x) * 180.f / M_PI);

        // Uncomment the following block if you want to normalize angles.y
        // if (angles.y > 90)
        //     angles.y -= 180;
        // else if (angles.y < -90)
        //     angles.y += 180;
    }

    angles.z = 0.0f;
    return angles;
}


float math::GetFOV( const ang_t &view_angles, const vec3_t &start, const vec3_t &end ) {
    vec3_t dir, fw;

    // get direction and normalize.
    dir = ( end - start ).normalized( );

    // get the forward direction vector of the view angles.
    AngleVectors( view_angles, &fw );

    // get the angle between the view angles forward directional vector and the target location.
    return std::max( rad_to_deg( std::acos( fw.dot( dir ) ) ), 0.f );
}

void math::VectorTransform( const vec3_t& in, const matrix3x4_t& matrix, vec3_t& out ) {
    out = {
        in.dot( vec3_t( matrix[ 0 ][ 0 ], matrix[ 0 ][ 1 ], matrix[ 0 ][ 2 ] ) ) + matrix[ 0 ][ 3 ],
        in.dot( vec3_t( matrix[ 1 ][ 0 ], matrix[ 1 ][ 1 ], matrix[ 1 ][ 2 ] ) ) + matrix[ 1 ][ 3 ],
        in.dot( vec3_t( matrix[ 2 ][ 0 ], matrix[ 2 ][ 1 ], matrix[ 2 ][ 2 ] ) ) + matrix[ 2 ][ 3 ]
    };
}

void math::VectorITransform( const vec3_t &in, const matrix3x4_t &matrix, vec3_t &out ) {
    vec3_t diff;

    diff = { 
        in.x - matrix[ 0 ][ 3 ], 
        in.y - matrix[ 1 ][ 3 ], 
        in.z - matrix[ 2 ][ 3 ] 
    };

    out = {
        diff.x * matrix[ 0 ][ 0 ] + diff.y * matrix[ 1 ][ 0 ] + diff.z * matrix[ 2 ][ 0 ],
        diff.x * matrix[ 0 ][ 1 ] + diff.y * matrix[ 1 ][ 1 ] + diff.z * matrix[ 2 ][ 1 ],
        diff.x * matrix[ 0 ][ 2 ] + diff.y * matrix[ 1 ][ 2 ] + diff.z * matrix[ 2 ][ 2 ]
    };
}

void math::MatrixAngles( const matrix3x4_t& matrix, ang_t& angles ) {
    vec3_t forward, left, up;

    // extract the basis vectors from the matrix. since we only need the z
    // component of the up vector, we don't get x and y.
    forward = { matrix[ 0 ][ 0 ], matrix[ 1 ][ 0 ], matrix[ 2 ][ 0 ] };
    left    = { matrix[ 0 ][ 1 ], matrix[ 1 ][ 1 ], matrix[ 2 ][ 1 ] };
    up      = { 0.f, 0.f, matrix[ 2 ][ 2 ] };

    float len = forward.length_2d( );

    // enough here to get angles?
    if( len > 0.001f ) {
        angles.x = rad_to_deg( std::atan2( -forward.z, len ) );
        angles.y = rad_to_deg( std::atan2( forward.y, forward.x ) );
        angles.z = rad_to_deg( std::atan2( left.z, up.z ) );
    }

    else {
        angles.x = rad_to_deg( std::atan2( -forward.z, len ) );
        angles.y = rad_to_deg( std::atan2( -left.x, left.y ) );
        angles.z = 0.f;
    }
}

void math::MatrixCopy( const matrix3x4_t &in, matrix3x4_t &out ) {
    std::memcpy( out.Base( ), in.Base( ), sizeof( matrix3x4_t ) );
}

vec3_t math::extrapolate_pos(vec3_t pos, vec3_t extension, int amount, float interval)
{
    return pos + (extension * (interval * amount));
}

float math::NormalizeYaw(float angle) {
    if (!std::isfinite(angle))
        angle = 0.f;

    return std::remainderf(angle, 360.0f);
}

void math::ConcatTransforms( const matrix3x4_t &in1, const matrix3x4_t &in2, matrix3x4_t &out ) {
    if( &in1 == &out ) {
        matrix3x4_t in1b;
        MatrixCopy( in1, in1b );
        ConcatTransforms( in1b, in2, out );
        return;
    }

    if( &in2 == &out ) {
        matrix3x4_t in2b;
        MatrixCopy( in2, in2b );
        ConcatTransforms( in1, in2b, out );
        return;
    }

    out[ 0 ][ 0 ] = in1[ 0 ][ 0 ] * in2[ 0 ][ 0 ] + in1[ 0 ][ 1 ] * in2[ 1 ][ 0 ] + in1[ 0 ][ 2 ] * in2[ 2 ][ 0 ];
    out[ 0 ][ 1 ] = in1[ 0 ][ 0 ] * in2[ 0 ][ 1 ] + in1[ 0 ][ 1 ] * in2[ 1 ][ 1 ] + in1[ 0 ][ 2 ] * in2[ 2 ][ 1 ];
    out[ 0 ][ 2 ] = in1[ 0 ][ 0 ] * in2[ 0 ][ 2 ] + in1[ 0 ][ 1 ] * in2[ 1 ][ 2 ] + in1[ 0 ][ 2 ] * in2[ 2 ][ 2 ];
    out[ 0 ][ 3 ] = in1[ 0 ][ 0 ] * in2[ 0 ][ 3 ] + in1[ 0 ][ 1 ] * in2[ 1 ][ 3 ] + in1[ 0 ][ 2 ] * in2[ 2 ][ 3 ] + in1[ 0 ][ 3 ];

    out[ 1 ][ 0 ] = in1[ 1 ][ 0 ] * in2[ 0 ][ 0 ] + in1[ 1 ][ 1 ] * in2[ 1 ][ 0 ] + in1[ 1 ][ 2 ] * in2[ 2 ][ 0 ];
    out[ 1 ][ 1 ] = in1[ 1 ][ 0 ] * in2[ 0 ][ 1 ] + in1[ 1 ][ 1 ] * in2[ 1 ][ 1 ] + in1[ 1 ][ 2 ] * in2[ 2 ][ 1 ];
    out[ 1 ][ 2 ] = in1[ 1 ][ 0 ] * in2[ 0 ][ 2 ] + in1[ 1 ][ 1 ] * in2[ 1 ][ 2 ] + in1[ 1 ][ 2 ] * in2[ 2 ][ 2 ];
    out[ 1 ][ 3 ] = in1[ 1 ][ 0 ] * in2[ 0 ][ 3 ] + in1[ 1 ][ 1 ] * in2[ 1 ][ 3 ] + in1[ 1 ][ 2 ] * in2[ 2 ][ 3 ] + in1[ 1 ][ 3 ];

    out[ 2 ][ 0 ] = in1[ 2 ][ 0 ] * in2[ 0 ][ 0 ] + in1[ 2 ][ 1 ] * in2[ 1 ][ 0 ] + in1[ 2 ][ 2 ] * in2[ 2 ][ 0 ];
    out[ 2 ][ 1 ] = in1[ 2 ][ 0 ] * in2[ 0 ][ 1 ] + in1[ 2 ][ 1 ] * in2[ 1 ][ 1 ] + in1[ 2 ][ 2 ] * in2[ 2 ][ 1 ];
    out[ 2 ][ 2 ] = in1[ 2 ][ 0 ] * in2[ 0 ][ 2 ] + in1[ 2 ][ 1 ] * in2[ 1 ][ 2 ] + in1[ 2 ][ 2 ] * in2[ 2 ][ 2 ];
    out[ 2 ][ 3 ] = in1[ 2 ][ 0 ] * in2[ 0 ][ 3 ] + in1[ 2 ][ 1 ] * in2[ 1 ][ 3 ] + in1[ 2 ][ 2 ] * in2[ 2 ][ 3 ] + in1[ 2 ][ 3 ];
}

bool math::IntersectRayWithBox( const vec3_t &start, const vec3_t &delta, const vec3_t &mins, const vec3_t &maxs, float tolerance, BoxTraceInfo_t *out_info ) {
    int   i;
    float d1, d2, f;

    for( i = 0; i < 6; ++i ) {
        if( i >= 3 ) {
            d1 = start[ i - 3 ] - maxs[ i - 3 ];
            d2 = d1 + delta[ i - 3 ];
        }
        
        else {
            d1 = -start[ i ] + mins[ i ];
            d2 = d1 - delta[ i ];
        }
        
        // if completely in front of face, no intersection.
        if( d1 > 0.f && d2 > 0.f ){
            out_info->m_startsolid = false;

            return false;
        }
        
        // completely inside, check next face.
        if( d1 <= 0.f && d2 <= 0.f )
            continue;
        
        if( d1 > 0.f )
            out_info->m_startsolid = false;
        
        // crosses face.
        if( d1 > d2 ) {
            f = std::max( 0.f, d1 - tolerance );

            f = f / ( d1 - d2 );
            if( f > out_info->m_t1 ) {
                out_info->m_t1      = f;
                out_info->m_hitside = i;
            }
        }
        
        // leave.
        else {
            f = ( d1 + tolerance ) / ( d1 - d2 );
            if( f < out_info->m_t2 )
                out_info->m_t2 = f;
        }
    }

    return out_info->m_startsolid || ( out_info->m_t1 < out_info->m_t2 && out_info->m_t1 >= 0.f );
}

bool math::IntersectRayWithBox( const vec3_t &start, const vec3_t &delta, const vec3_t &mins, const vec3_t &maxs, float tolerance, CBaseTrace *out_tr, float *fraction_left_solid ) {
    BoxTraceInfo_t box_tr;
    
    // note - dex; this is Collision_ClearTrace.
    out_tr->m_startpos   = start;
	out_tr->m_endpos     = start;
	out_tr->m_endpos    += delta;
	out_tr->m_startsolid = false;
	out_tr->m_allsolid   = false;
	out_tr->m_fraction   = 1.f;
	out_tr->m_contents   = 0;

    if( IntersectRayWithBox( start, delta, mins, maxs, tolerance, &box_tr ) ) {
        out_tr->m_startsolid = box_tr.m_startsolid;
        
        if( box_tr.m_t1 < box_tr.m_t2 && box_tr.m_t1 >= 0.f ){
        	out_tr->m_fraction = box_tr.m_t1;
        
        	// VectorMA( pTrace->startpos, trace.t1, vecRayDelta, pTrace->endpos );
        
        	out_tr->m_contents       = CONTENTS_SOLID;
            out_tr->m_plane.m_normal = vec3_t{};
        
        	if( box_tr.m_hitside >= 3 ) {
        		box_tr.m_hitside -= 3;
        
        		out_tr->m_plane.m_dist                       = maxs[ box_tr.m_hitside ];
        		out_tr->m_plane.m_normal[ box_tr.m_hitside ] = 1.f;
        		out_tr->m_plane.m_type                       = box_tr.m_hitside;
        	}
        	
            else {
        		out_tr->m_plane.m_dist                       = -mins[ box_tr.m_hitside ];
        		out_tr->m_plane.m_normal[ box_tr.m_hitside ] = -1.f;
        		out_tr->m_plane.m_type                       = box_tr.m_hitside;
        	}
        
        	return true;
        }
        
        if( out_tr->m_startsolid ) {
        	out_tr->m_allsolid = ( box_tr.m_t2 <= 0.f ) || ( box_tr.m_t2 >= 1.f );
        	out_tr->m_fraction = 0.f;
        
        	if( fraction_left_solid )
                *fraction_left_solid = box_tr.m_t2;
        
        	out_tr->m_endpos         = out_tr->m_startpos;
        	out_tr->m_contents       = CONTENTS_SOLID;
        	out_tr->m_plane.m_dist   = out_tr->m_startpos.x;
        	out_tr->m_plane.m_normal = { 1.f, 0.f, 0.f };
        	out_tr->m_plane.m_type   = 0;
        	out_tr->m_startpos       = start + ( box_tr.m_t2 * delta );
        
        	return true;
        }
    }

    return false;
}

bool math::IntersectRayWithOBB( const vec3_t &start, const vec3_t &delta, const matrix3x4_t &obb_to_world, const vec3_t &mins, const vec3_t &maxs, float tolerance, CBaseTrace *out_tr ) {
    vec3_t box_extents, box_center, extent{}, uextent, segment_center, cross, new_start, tmp_end;
    float  coord, tmp, cextent, sign;

    // note - dex; this is Collision_ClearTrace.
    out_tr->m_startpos   = start;
	out_tr->m_endpos     = start;
	out_tr->m_endpos    += delta;
	out_tr->m_startsolid = false;
	out_tr->m_allsolid   = false;
	out_tr->m_fraction   = 1.f;
	out_tr->m_contents   = 0;

    // compute center in local space and transform to world space.
    box_extents = ( mins + maxs ) / 2.f;
    VectorTransform( box_extents, obb_to_world, box_center );

    // calculate extents from local center.
    box_extents = maxs - box_extents;

    // save the extents of the ray.
    segment_center = start + delta - box_center;

    // check box axes for separation.
	for( int i = 0; i < 3; ++i ){
		extent[ i ]  = delta.x * obb_to_world[ 0 ][ i ] + delta.y * obb_to_world[ 1 ][ i ] + delta.z * obb_to_world[ 2 ][ i ];
		uextent[ i ] = std::abs( extent[ i ] );

		coord = segment_center.x * obb_to_world[ 0 ][ i ] + segment_center.y * obb_to_world[ 1 ][ i ] + segment_center.z * obb_to_world[ 2 ][ i ];
		coord = std::abs( coord );
		if( coord > ( box_extents[ i ] + uextent[ i ] ) )
			return false;
	}

    // now check cross axes for separation.
    cross = delta.cross( segment_center );

    cextent = cross.x * obb_to_world[ 0 ][ 0 ] + cross.y * obb_to_world[ 1 ][ 0 ] + cross.z * obb_to_world[ 2 ][ 0 ];
    cextent = std::abs( cextent );
    tmp     = box_extents.y * uextent.z + box_extents.z * uextent.y;
    if( cextent > tmp )
    	return false;
    
    cextent = cross.x * obb_to_world[ 0 ][ 1 ] + cross.y * obb_to_world[ 1 ][ 1 ] + cross.z * obb_to_world[ 2 ][ 1 ];
    cextent = std::abs( cextent );
    tmp     = box_extents.x * uextent.z + box_extents.z * uextent.x;
    if( cextent > tmp )
    	return false;
    
    cextent = cross.x * obb_to_world[ 0 ][ 2 ] + cross.y * obb_to_world[ 1 ][ 2 ] + cross.z * obb_to_world[ 2 ][ 2 ];
    cextent = std::abs( cextent );
    tmp     = box_extents.x * uextent.y + box_extents.y * uextent.x;
    if( cextent > tmp )
    	return false;

    // we hit this box, compute intersection point and return.
    // compute ray start in bone space.
	VectorITransform( start, obb_to_world, new_start );

    // extent is ray.m_Delta in bone space, recompute delta in bone space.
    extent *= 2.f;

    // delta was prescaled by the current t, so no need to see if this intersection is closer.
    if( !IntersectRayWithBox( start, extent, mins, maxs, tolerance, out_tr ) )
        return false;

    // fix up the start/end pos and fraction
    VectorTransform( out_tr->m_endpos, obb_to_world, tmp_end );

    out_tr->m_endpos    = tmp_end;
    out_tr->m_startpos  = start;
    out_tr->m_fraction *= 2.f;
    
    // fix up the plane information
    sign = out_tr->m_plane.m_normal[ out_tr->m_plane.m_type ];

    out_tr->m_plane.m_normal.x = sign * obb_to_world[ 0 ][ out_tr->m_plane.m_type ];
    out_tr->m_plane.m_normal.y = sign * obb_to_world[ 1 ][ out_tr->m_plane.m_type ];
    out_tr->m_plane.m_normal.z = sign * obb_to_world[ 2 ][ out_tr->m_plane.m_type ];
    out_tr->m_plane.m_dist     = out_tr->m_endpos.dot( out_tr->m_plane.m_normal );
    out_tr->m_plane.m_type     = 3;

    return true;
}

bool math::IntersectRayWithOBB( const vec3_t &start, const vec3_t &delta, const vec3_t &box_origin, const ang_t &box_rotation, const vec3_t &mins, const vec3_t &maxs, float tolerance, CBaseTrace *out_tr ) {
    // todo - dex; https://github.com/pmrowla/hl2sdk-csgo/blob/master/public/collisionutils.cpp#L1400
    return false;
}

bool math::IntersectInfiniteRayWithSphere( const vec3_t &start, const vec3_t &delta, const vec3_t &sphere_center, float radius, float *out_t1, float *out_t2 ) {
    vec3_t sphere_to_ray;
    float  a, b, c, discrim, oo2a;

    sphere_to_ray = start - sphere_center;
    a             = delta.dot( delta ); 
    
    // this would occur in the case of a zero-length ray.
    if( !a ) {
        *out_t1 = 0.f;
        *out_t2 = 0.f;

        return sphere_to_ray.length_sqr( ) <= ( radius * radius );
    }

    b = 2.f * sphere_to_ray.dot( delta );
    c = sphere_to_ray.dot( sphere_to_ray ) - ( radius * radius );

    discrim = b * b - 4.f * a * c;
    if( discrim < 0.f )
        return false;

    discrim = std::sqrt( discrim );
    oo2a    = 0.5f / a;

    *out_t1 = ( -b - discrim ) * oo2a;
    *out_t2 = ( -b + discrim ) * oo2a;

    return true;
}

bool math::IntersectRayWithSphere( const vec3_t &start, const vec3_t &delta, const vec3_t &sphere_center, float radius, float *out_t1, float *out_t2 ) {
    if( !IntersectInfiniteRayWithSphere( start, delta, sphere_center, radius, out_t1, out_t2 ) )
        return false;

    if( *out_t1 > 1.0f || *out_t2 < 0.0f )
        return false;

    // clamp intersection points.
    *out_t1 = std::max( 0.f, *out_t1 );
    *out_t2 = std::min( 1.f, *out_t2 );

    return true;
}

vec3_t math::Interpolate( const vec3_t from, const vec3_t to, const float percent ) {
    return to * percent + from * ( 1.f - percent );
}

vec3_t math::CalcAngle(const vec3_t& vecSource, const vec3_t& vecDestination) {
    vec3_t vAngle;
    vec3_t delta((vecSource.x - vecDestination.x), (vecSource.y - vecDestination.y), (vecSource.z - vecDestination.z));
    double hyp = sqrt(delta.x * delta.x + delta.y * delta.y);

    vAngle.x = float(atanf(float(delta.z / hyp)) * 57.295779513082f);
    vAngle.y = float(atanf(float(delta.y / delta.x)) * 57.295779513082f);
    vAngle.z = 0.0f;

    if (delta.x >= 0.0)
        vAngle.y += 180.0f;

    return vAngle;
}

bool math::IntersectSegmentToSegment(vec3_t s1, vec3_t s2, vec3_t k1, vec3_t k2, float radius) {
    static auto constexpr epsilon = 0.00000001;

    auto u = s2 - s1;
    auto v = k2 - k1;
    const auto w = s1 - k1;

    const auto a = u.dot(u);
    const auto b = u.dot(v);
    const auto c = v.dot(v);
    const auto d = u.dot(w);
    const auto e = v.dot(w);
    const auto D = a * c - b * b;
    float sn, sd = D;
    float tn, td = D;

    if (D < epsilon) {
        sn = 0.0;
        sd = 1.0;
        tn = e;
        td = c;
    }
    else {
        sn = b * e - c * d;
        tn = a * e - b * d;

        if (sn < 0.0) {
            sn = 0.0;
            tn = e;
            td = c;
        }
        else if (sn > sd) {
            sn = sd;
            tn = e + b;
            td = c;
        }
    }

    if (tn < 0.0) {
        tn = 0.0;

        if (-d < 0.0)
            sn = 0.0;
        else if (-d > a)
            sn = sd;
        else {
            sn = -d;
            sd = a;
        }
    }
    else if (tn > td) {
        tn = td;

        if (-d + b < 0.0)
            sn = 0;
        else if (-d + b > a)
            sn = sd;
        else {
            sn = -d + b;
            sd = a;
        }
    }

    const float sc = abs(sn) < epsilon ? 0.0 : sn / sd;
    const float tc = abs(tn) < epsilon ? 0.0 : tn / td;

    m128 n;
    auto dp = w + u * sc - v * tc;
    n.f[0] = dp.dot(dp);
    const auto calc = sqrt_ps(n.v);
    auto shit = reinterpret_cast<const m128*>(&calc)->f[0];
    //printf( "shit %f | rad %f\n", shit, radius );
    return shit < radius;
}

bool math::IntersectionBoundingBox(const vec3_t& src, const vec3_t& dir, const vec3_t& min, const vec3_t& max, vec3_t* hit_point) {
    /*
         Fast Ray-Box Intersection
         by Andrew Woo
         from "Graphics Gems", Academic Press, 1990
     */

    constexpr auto NUMDIM = 3;
    constexpr auto RIGHT = 0;
    constexpr auto LEFT = 1;
    constexpr auto MIDDLE = 2;

    bool inside = true;
    char quadrant[NUMDIM];
    int i;

    // Rind candidate planes; this loop can be avoided if
    // rays cast all from the eye(assume perpsective view)
    vec3_t candidatePlane;
    for (i = 0; i < NUMDIM; i++) {
        if (src[i] < min[i]) {
            quadrant[i] = LEFT;
            candidatePlane[i] = min[i];
            inside = false;
        }
        else if (src[i] > max[i]) {
            quadrant[i] = RIGHT;
            candidatePlane[i] = max[i];
            inside = false;
        }
        else {
            quadrant[i] = MIDDLE;
        }
    }

    // Ray origin inside bounding box
    if (inside) {
        if (hit_point)
            *hit_point = src;
        return true;
    }

    // Calculate T distances to candidate planes
    vec3_t maxT;
    for (i = 0; i < NUMDIM; i++) {
        if (quadrant[i] != MIDDLE && dir[i] != 0.f)
            maxT[i] = (candidatePlane[i] - src[i]) / dir[i];
        else
            maxT[i] = -1.f;
    }

    // Get largest of the maxT's for final choice of intersection
    int whichPlane = 0;
    for (i = 1; i < NUMDIM; i++) {
        if (maxT[whichPlane] < maxT[i])
            whichPlane = i;
    }

    // Check final candidate actually inside box
    if (maxT[whichPlane] < 0.f)
        return false;

    for (i = 0; i < NUMDIM; i++) {
        if (whichPlane != i) {
            float temp = src[i] + maxT[whichPlane] * dir[i];
            if (temp < min[i] || temp > max[i]) {
                return false;
            }
            else if (hit_point) {
                (*hit_point)[i] = temp;
            }
        }
        else if (hit_point) {
            (*hit_point)[i] = candidatePlane[i];
        }
    }

    // ray hits box
    return true;
}


float math::AngleDiff(float destAngle, float srcAngle)
{
    float delta = fmodf(destAngle - srcAngle, 360.0f);

    if (destAngle > srcAngle)
    {
        if (delta >= 180)
            delta -= 360;
    }
    else
    {
        if (delta <= -180)
            delta += 360;
    }

    return delta;
}