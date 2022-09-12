#include "../Common/common.hpp"


/**
 * @param BA vertex B' pos minus vertex A' pos
 */
inline vec3f ComputeTangent(const vec3f &BA, const vec3f CA,
                            const vec2f &uvBA, const vec2f &uvCA,
                            const vec3f &normal) {
    const float m00 = uvBA.x, m01 = uvBA.y;
    const float m10 = uvCA.x, m11 = uvCA.y;
    const float det = m00 * m11 - m01 * m10;
    if (std::abs(det) < 0.0001f)
        return wzz::math::tcoord3<float>::from_z(normal).x;
    const float inv_det = 1 / det;
    return (m11 * inv_det * BA - m01 * inv_det * CA).normalized();
}

class PMApp final : public gl_app_t{
public:
    using gl_app_t::gl_app_t;
private:
    void initialize() override {
        GL_EXPR(glEnable(GL_DEPTH_TEST));
        GL_EXPR(glClearColor(0, 0, 0, 0));
        GL_EXPR(glClearDepth(1.0));

        auto load_model = [&](std::string path, std::string depth_path, std::string albedo_path, std::string normal_path){
            auto model = load_model_from_obj_file(path);

            auto normal = wzz::image::load_rgb_from_file(normal_path);
            auto albedo = wzz::image::load_rgb_from_file(albedo_path);
            auto depth  = wzz::image::load_rgb_from_file(depth_path);
            auto normal_tex = std::make_shared<texture2d_t>(true);
            auto albedo_tex = std::make_shared<texture2d_t>(true);
            auto depth_tex  = std::make_shared<texture2d_t>(true);
            normal_tex->initialize_format_and_data(1, GL_RGB8, normal);
            albedo_tex->initialize_format_and_data(1, GL_RGB8, albedo);
            depth_tex->initialize_format_and_data(1,GL_RGB8, depth);
            for(auto& mesh : model->meshes){
                auto& m = draw_meshes.emplace_back();
                m.model = transform::scale(vec3(0.2f));
                m.normal_tex = normal_tex;
                m.albedo_tex = albedo_tex;
                m.depth_tex = depth_tex;
                m.vao.initialize_handle();
                m.vbo.initialize_handle();
                m.ebo.initialize_handle();
                std::vector<Vertex> vertices;
                int n = mesh.indices.size() / 3;
                auto &indices = mesh.indices;
                for (int i = 0; i < n; i++) {
                    const auto &A = mesh.vertices[indices[3 * i]];
                    const auto &B = mesh.vertices[indices[3 * i + 1]];
                    const auto &C = mesh.vertices[indices[3 * i + 2]];
                    const auto BA = B.pos - A.pos;
                    const auto CA = C.pos - A.pos;
                    const auto uvBA = B.tex_coord - A.tex_coord;
                    const auto uvCA = C.tex_coord - A.tex_coord;
                    for (int j = 0; j < 3; j++) {
                        const auto &v = mesh.vertices[indices[3 * i + j]];
                        vertices.push_back({v.pos, v.normal,v.tex_coord, ComputeTangent(BA, CA, uvBA, uvCA, v.normal)});
                    }
                }
                assert(vertices.size() == mesh.vertices.size());
                m.vbo.reinitialize_buffer_data(vertices.data(), vertices.size(), GL_STATIC_DRAW);
                m.ebo.reinitialize_buffer_data(indices.data(), indices.size(), GL_STATIC_DRAW);
                m.vao.bind_vertex_buffer_to_attrib(attrib_var_t<vec3f>(0), m.vbo, &Vertex::pos, 0);
                m.vao.bind_vertex_buffer_to_attrib(attrib_var_t<vec3f>(1), m.vbo, &Vertex::normal, 1);
                m.vao.bind_vertex_buffer_to_attrib(attrib_var_t<vec2f>(2), m.vbo, &Vertex::tex_coord,2);
                m.vao.bind_vertex_buffer_to_attrib(attrib_var_t<vec3f>(3), m.vbo, &Vertex::tangent, 3);

                m.vao.enable_attrib(attrib_var_t<vec3f>(0));
                m.vao.enable_attrib(attrib_var_t<vec3f>(1));
                m.vao.enable_attrib(attrib_var_t<vec2f>(2));
                m.vao.enable_attrib(attrib_var_t<vec3f>(3));
                m.vao.bind_index_buffer(m.ebo);
            }
        };
        std::string path = "./asset/ParallaxMapping/";
        load_model(path + "ground.obj", path + "toy_box_disp.png", path + "toy_box_diffuse.png", path + "toy_box_normal.png");

        pm_shader = program_t::build_from(
                shader_t<GL_VERTEX_SHADER>::from_file(path + "pm.vert"),
                shader_t<GL_FRAGMENT_SHADER>::from_file(path + "pm.frag"));

        transform_buffer.initialize_handle();
        transform_buffer.reinitialize_buffer_data(nullptr, GL_STATIC_DRAW);

        per_frame_buffer.initialize_handle();
        per_frame_buffer.reinitialize_buffer_data(nullptr, GL_STATIC_DRAW);

        params_buffer.initialize_handle();
        params_buffer.reinitialize_buffer_data(&params, GL_STATIC_DRAW);

        camera.set_position({ 0, 2, 3 });
        camera.set_direction(-3.14159f * 0.5f, -0.6f);
        camera.set_perspective(60.0f, 0.1f, 100.0f);
    }

    void frame() override {
        handle_events();
        //gui
        if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Press LCtrl to show/hide cursor");
            ImGui::Text("Use W/A/S/D/Space/LShift to move");
            if (ImGui::Checkbox("VSync", &vsync))
                window->set_vsync(vsync);
            ImGui::Text("FPS: %.0f", ImGui::GetIO().Framerate);

            bool update = false;
            update |= ImGui::InputFloat3("Light Position", &params.light_pos.x);
            update |= ImGui::SliderFloat("Height Scale", &params.height_scale, 0.01f, 0.2f);
            if(ImGui::RadioButton("Normal Mapping", mp == Normal_Mapping)){
                mp = Normal_Mapping;
                update = true;
            }
            if(ImGui::RadioButton("Parallax Occlusion Mapping", mp == Parallax_Occlusion_Mapping)){
                mp = Parallax_Occlusion_Mapping;
                update = true;
            }
            if(ImGui::RadioButton("Relief Parallax Mapping", mp == Relief_Parallax_Mapping)){
                mp = Relief_Parallax_Mapping;
                update = true;
            }
            if(update){
                params.map_method = static_cast<int>(mp);
                params_buffer.set_buffer_data(&params);
            }
        }

        framebuffer_t::bind_to_default();
        framebuffer_t::clear_color_depth_buffer();

        auto proj_view = camera.get_view_proj();

        per_frame.view_pos = camera.get_position();
        per_frame_buffer.set_buffer_data(&per_frame);

        pm_shader.bind();
        transform_buffer.bind(0);
        per_frame_buffer.bind(1);
        params_buffer.bind(2);
        for(auto& draw_mesh : draw_meshes){
            transform.model = draw_mesh.model;
            transform.mvp = proj_view * draw_mesh.model;
            transform_buffer.set_buffer_data(&transform);

            draw_mesh.depth_tex->bind(0);
            draw_mesh.albedo_tex->bind(1);
            draw_mesh.normal_tex->bind(2);
            draw_mesh.vao.bind();

            GL_EXPR(glDrawElements(GL_TRIANGLES, draw_mesh.ebo.index_count(), GL_UNSIGNED_INT, nullptr));

            draw_mesh.vao.unbind();
        }


        pm_shader.unbind();

        ImGui::End();
    }

    void destroy() override {

    }
private:
    bool vsync = true;

    struct Vertex{
        vec3 pos;
        vec3 normal;
        vec2 tex_coord;
        vec3 tangent;
    };
    struct DrawMesh{
        mat4 model;
        vertex_array_t vao;
        vertex_buffer_t<Vertex> vbo;
        index_buffer_t<uint32_t> ebo;
        std::shared_ptr<texture2d_t> depth_tex;
        std::shared_ptr<texture2d_t> albedo_tex;
        std::shared_ptr<texture2d_t> normal_tex;
    };
    std::vector<DrawMesh> draw_meshes;

    struct alignas(16) Transform{
        mat4 model;
        mat4 mvp;
    }transform;
    std140_uniform_block_buffer_t<Transform> transform_buffer;

    struct alignas(16) PerFrame{
        vec3 view_pos;
    }per_frame;
    std140_uniform_block_buffer_t<PerFrame> per_frame_buffer;

    enum MapMethod : int{
        Normal_Mapping = 0,
        Parallax_Occlusion_Mapping = 1,
        Relief_Parallax_Mapping = 2
    };
    MapMethod mp = Relief_Parallax_Mapping;
    struct alignas(16) Params{
        vec3 light_pos = vec3(5, 5, 0);
        float height_scale = 0.1f;
        vec3 light_radiance = vec3(1.f);
        int map_method = 2;
    }params;
    std140_uniform_block_buffer_t<Params> params_buffer;


    program_t pm_shader;

};

int main(){
    PMApp(window_desc_t{
            .size = {1600, 900},
            .title = "PM",
            .multisamples = 4
    }).run();
}