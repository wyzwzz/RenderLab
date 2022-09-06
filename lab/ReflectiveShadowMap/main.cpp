#include "../Common/common.hpp"

class RSMApp final : public gl_app_t{
public:
    using gl_app_t::gl_app_t;
private:
    void initialize() override {

    }

    void frame() override {
        handle_events();


    }

    void destroy() override {

    }


private:
    vertex_array_t quad_vao;

    struct Vertex{
        vec3 pos;
        vec3 normal;
        vec3 albedo;
    };
    struct DrawMesh{
        mat4 model;
        vertex_array_t vao;
        vertex_buffer_t<Vertex> vbo;
        index_buffer_t<uint32_t> ebo;
    };

    std::vector<DrawMesh> draw_meshes;

    struct Transform{
        mat4 model;
        mat4 mvp;
    }transform;
    std140_uniform_block_buffer_t<Transform> transform_buffer;

    struct RSMResc{
        texture2d_t pos;
        texture2d_t normal;
        texture2d_t albedo;
    }rsm_resc;

    struct Light{
        vec3 light_direction; float pad0;
        vec3 light_radiance;  float pad1;
    }light;
    std140_uniform_block_buffer_t<Light> light_buffer;

    struct GBuffer{
        texture2d_t gbuffer0;
        texture2d_t gbuffer1;
    };
    GBuffer gbuffer_resc;
    GBuffer low_res_guffer_resc;

    texture2d_t lowres_indirect_tex;

    struct IndirectParams{
        mat4 rsm_light_viewproj;
        int poission_sample_count;
        float indirect_sample_radius;
        float light_proj_world_area;
        int enable_indirect;
    }indrect_params;
    std140_uniform_block_buffer_t<IndirectParams> indirect_params_buffer;


    storage_buffer_t<vec4> possion_disk_samples_buffer;

    program_t rsm;
    program_t gbuffer;
    program_t gbuffer_lowres;
    program_t lowres_indirect;
    program_t rsm_shading;
};

int main(){
    RSMApp(window_desc_t{
            .size = {1600, 900},
            .title = "RSM",
            .multisamples = 4
    }).run();
}