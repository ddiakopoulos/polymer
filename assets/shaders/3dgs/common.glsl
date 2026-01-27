#define TILE_WIDTH 16
#define TILE_HEIGHT 16
#define SH_MAX_COEFFS 48

const float SH_C0 = 0.28209479177387814;
const float SH_C1 = 0.4886025119029199;
const float SH_C2[5] = float[5](
    1.0925484305920792,
    -1.0925484305920792,
    0.31539156525252005,
    -1.0925484305920792,
    0.5462742152960396
);
const float SH_C3[7] = float[7](
    -0.5900435899266435,
    2.890611442640554,
    -0.4570457994644658,
    0.3731763325901154,
    -0.4570457994644658,
    1.445305721320277,
    -0.5900435899266435
);

struct Vertex {
    vec4 position;
    vec4 scale_opacity;
    vec4 rotation;
    float sh[48];
};

struct VertexAttribute {
    vec4 conic_opacity;   // conic.xx, conic.xy, conic.yy, opacity
    vec4 color_radii;     // rgb color, screen-space radius
    uvec4 aabb;           // tile bounding box (min_x, min_y, max_x, max_y)
    vec2 uv;              // screen-space center
    float depth;          // view-space depth
    uint pad;
};

mat3 rotationFromQuaternion(vec4 q) {
    // q is stored as (w, x, y, z) in our data
    float qw = q.x;
    float qx = q.y;
    float qy = q.z;
    float qz = q.w;

    float qx2 = qx * qx;
    float qy2 = qy * qy;
    float qz2 = qz * qz;

    mat3 R;
    R[0][0] = 1.0 - 2.0 * qy2 - 2.0 * qz2;
    R[0][1] = 2.0 * qx * qy - 2.0 * qz * qw;
    R[0][2] = 2.0 * qx * qz + 2.0 * qy * qw;

    R[1][0] = 2.0 * qx * qy + 2.0 * qz * qw;
    R[1][1] = 1.0 - 2.0 * qx2 - 2.0 * qz2;
    R[1][2] = 2.0 * qy * qz - 2.0 * qx * qw;

    R[2][0] = 2.0 * qx * qz - 2.0 * qy * qw;
    R[2][1] = 2.0 * qy * qz + 2.0 * qx * qw;
    R[2][2] = 1.0 - 2.0 * qx2 - 2.0 * qy2;

    return R;
}
