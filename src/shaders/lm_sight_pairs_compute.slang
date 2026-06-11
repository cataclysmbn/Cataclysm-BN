// GPU Lighting - Batched terrain LOS queries

static const float LIGHT_TRANSPARENCY_SOLID = 0.0;
static const uint SIGHT_PAIR_THREADS_X = 64u;

struct GpuSightPair
{
    int from_x;
    int from_y;
    int from_z_idx;
    int to_x;
    int to_y;
    int to_z_idx;
    int range;
    uint _pad0;
};

[[vk::binding(0, 2)]]
cbuffer Constants : register(b0, space2)
{
    int cache_x;
    int cache_y;
    int cache_xy;
    int z_count;
    uint pair_count;
    uint _pad0;
    uint _pad1;
    uint _pad2;
};

[[vk::binding(0, 0)]]
StructuredBuffer<float> transparency_all : register(t0, space0);

[[vk::binding(1, 0)]]
StructuredBuffer<uint> floor_all : register(t1, space0);

[[vk::binding(2, 0)]]
StructuredBuffer<GpuSightPair> pairs : register(t2, space0);

[[vk::binding(0, 1)]]
RWStructuredBuffer<uint> results : register(u0, space1);

int tile_index( int x, int y, int z )
{
    return z * cache_xy + x * cache_y + y;
}

bool inbounds_tile( int x, int y, int z )
{
    return x >= 0 && y >= 0 && z >= 0 && x < cache_x && y < cache_y && z < z_count;
}

bool is_transparent_at( int x, int y, int z )
{
    if( !inbounds_tile( x, y, z ) ) {
        return false;
    }
    return transparency_all[tile_index( x, y, z )] > LIGHT_TRANSPARENCY_SOLID;
}

bool has_floor_at( int x, int y, int z )
{
    if( !inbounds_tile( x, y, z ) ) {
        return false;
    }
    return floor_all[tile_index( x, y, z )] != 0u;
}

int sgn( int value )
{
    return value == 0 ? 0 : ( value > 0 ? 1 : -1 );
}

int largest_abs_3d( int dx, int dy, int dz )
{
    int best = abs( dx );
    int ay = abs( dy );
    if( ay > best ) {
        best = ay;
    }
    int az = abs( dz );
    if( az > best ) {
        best = az;
    }
    return best;
}

bool check_2d_step( int x, int y, int z, int target_x, int target_y )
{
    if( x == target_x && y == target_y ) {
        return true;
    }
    return is_transparent_at( x, y, z );
}

bool check_3d_step( int3 current, int3 last_point, int3 target )
{
    if( current.x == target.x && current.y == target.y && current.z == target.z &&
        last_point.z == target.z ) {
        return true;
    }

    if( current.z == last_point.z ) {
        return is_transparent_at( current.x, current.y, current.z );
    }

    int high_z = current.z > last_point.z ? current.z : last_point.z;
    bool current_side_blocked =
        has_floor_at( current.x, current.y, high_z ) ||
        !is_transparent_at( current.x, current.y, last_point.z );
    bool last_side_blocked =
        has_floor_at( last_point.x, last_point.y, high_z ) ||
        !is_transparent_at( last_point.x, last_point.y, current.z );
    return !( current_side_blocked && last_side_blocked );
}

bool los_2d( GpuSightPair pair )
{
    int dx = pair.to_x - pair.from_x;
    int dy = pair.to_y - pair.from_y;
    int sx = sgn( dx );
    int sy = sgn( dy );
    int ax = abs( dx ) * 2;
    int ay = abs( dy ) * 2;
    int2 current = int2( pair.from_x, pair.from_y );
    int t = 0;

    if( ax == ay ) {
        while( current.x != pair.to_x ) {
            current.y += sy;
            current.x += sx;
            if( !check_2d_step( current.x, current.y, pair.from_z_idx, pair.to_x, pair.to_y ) ) {
                return false;
            }
        }
    } else if( ax > ay ) {
        while( current.x != pair.to_x ) {
            if( t > 0 ) {
                current.y += sy;
                t -= ax;
            }
            current.x += sx;
            t += ay;
            if( !check_2d_step( current.x, current.y, pair.from_z_idx, pair.to_x, pair.to_y ) ) {
                return false;
            }
        }
    } else {
        while( current.y != pair.to_y ) {
            if( t > 0 ) {
                current.x += sx;
                t -= ay;
            }
            current.y += sy;
            t += ax;
            if( !check_2d_step( current.x, current.y, pair.from_z_idx, pair.to_x, pair.to_y ) ) {
                return false;
            }
        }
    }

    return true;
}

bool los_3d( GpuSightPair pair )
{
    int3 target = int3( pair.to_x, pair.to_y, pair.to_z_idx );
    int3 current = int3( pair.from_x, pair.from_y, pair.from_z_idx );
    int3 last_point = current;

    int3 d = target - current;
    int3 s = int3( sgn( d.x ), sgn( d.y ), sgn( d.z ) );
    int3 a = int3( abs( d.x ) * 2, abs( d.y ) * 2, abs( d.z ) * 2 );
    int t = 0;
    int t2 = 0;

    if( a.x == a.y && a.y == a.z ) {
        while( current.x != target.x ) {
            current.z += s.z;
            current.y += s.y;
            current.x += s.x;
            if( !check_3d_step( current, last_point, target ) ) {
                return false;
            }
            last_point = current;
        }
    } else if( ( a.z > a.x ) && ( a.z > a.y ) ) {
        while( current.z != target.z ) {
            if( t > 0 ) {
                current.x += s.x;
                t -= a.z;
            }
            if( t2 > 0 ) {
                current.y += s.y;
                t2 -= a.z;
            }
            current.z += s.z;
            t += a.x;
            t2 += a.y;
            if( !check_3d_step( current, last_point, target ) ) {
                return false;
            }
            last_point = current;
        }
    } else if( ( a.x > a.y ) && ( a.x > a.z ) ) {
        while( current.x != target.x ) {
            if( t > 0 ) {
                current.y += s.y;
                t -= a.x;
            }
            if( t2 > 0 ) {
                current.z += s.z;
                t2 -= a.x;
            }
            current.x += s.x;
            t += a.y;
            t2 += a.z;
            if( !check_3d_step( current, last_point, target ) ) {
                return false;
            }
            last_point = current;
        }
    } else if( ( a.y > a.z ) && ( a.y > a.x ) ) {
        while( current.y != target.y ) {
            if( t > 0 ) {
                current.z += s.z;
                t -= a.y;
            }
            if( t2 > 0 ) {
                current.x += s.x;
                t2 -= a.y;
            }
            current.y += s.y;
            t += a.z;
            t2 += a.x;
            if( !check_3d_step( current, last_point, target ) ) {
                return false;
            }
            last_point = current;
        }
    } else if( a.x == a.y ) {
        while( current.x != target.x ) {
            if( t > 0 ) {
                current.z += s.z;
                t -= a.x;
            }
            current.y += s.y;
            current.x += s.x;
            t += a.z;
            if( !check_3d_step( current, last_point, target ) ) {
                return false;
            }
            last_point = current;
        }
    } else if( a.x == a.z ) {
        while( current.x != target.x ) {
            if( t > 0 ) {
                current.y += s.y;
                t -= a.x;
            }
            current.z += s.z;
            current.x += s.x;
            t += a.y;
            if( !check_3d_step( current, last_point, target ) ) {
                return false;
            }
            last_point = current;
        }
    } else if( a.y == a.z ) {
        while( current.y != target.y ) {
            if( t > 0 ) {
                current.x += s.x;
                t -= a.z;
            }
            current.y += s.y;
            current.z += s.z;
            t += a.x;
            if( !check_3d_step( current, last_point, target ) ) {
                return false;
            }
            last_point = current;
        }
    } else if( a.x > a.y ) {
        while( current.x != target.x ) {
            if( t > 0 ) {
                current.y += s.y;
                t -= a.x;
            }
            if( t2 > 0 ) {
                current.z += s.z;
                t2 -= a.x;
            }
            current.x += s.x;
            t += a.y;
            t2 += a.z;
            if( !check_3d_step( current, last_point, target ) ) {
                return false;
            }
            last_point = current;
        }
    } else {
        while( current.y != target.y ) {
            if( t > 0 ) {
                current.x += s.x;
                t -= a.y;
            }
            if( t2 > 0 ) {
                current.z += s.z;
                t2 -= a.y;
            }
            current.y += s.y;
            t += a.x;
            t2 += a.z;
            if( !check_3d_step( current, last_point, target ) ) {
                return false;
            }
            last_point = current;
        }
    }

    return true;
}

void sight_pairs_main_impl( uint3 group_id, uint3 group_thread_id )
{
    uint pair_idx = group_id.x * SIGHT_PAIR_THREADS_X + group_thread_id.x;
    if( pair_idx >= pair_count ) {
        return;
    }

    GpuSightPair pair = pairs[pair_idx];
    if( !inbounds_tile( pair.from_x, pair.from_y, pair.from_z_idx ) ||
        !inbounds_tile( pair.to_x, pair.to_y, pair.to_z_idx ) ) {
        results[pair_idx] = 0u;
        return;
    }

    int dist = largest_abs_3d(
        pair.to_x - pair.from_x,
        pair.to_y - pair.from_y,
        pair.to_z_idx - pair.from_z_idx );
    if( pair.range >= 0 && pair.range < dist ) {
        results[pair_idx] = 0u;
        return;
    }

    bool visible = pair.from_z_idx == pair.to_z_idx ? los_2d( pair ) : los_3d( pair );
    results[pair_idx] = visible ? 1u : 0u;
}

[numthreads(64, 1, 1)]
void main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    sight_pairs_main_impl( group_id, group_thread_id );
}

[numthreads(64, 1, 1)]
void cpu_main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID )
{
    sight_pairs_main_impl( group_id, group_thread_id );
}
