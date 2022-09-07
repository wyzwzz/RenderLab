#include "../Common/common.hpp"

class RSMApp final : public gl_app_t{
public:
    using gl_app_t::gl_app_t;
private:
    auto getLight() {
        float light_x_rad = wzz::math::deg2rad(light_x_degree);
        float light_y_rad = wzz::math::deg2rad(light_y_degree);
        static float light_dist = 35.f;
        float light_y = std::sin(light_y_rad);
        float light_x = std::cos(light_y_rad) * std::cos(light_x_rad);
        float light_z = std::cos(light_y_rad) * std::sin(light_x_rad);
        vec3f light_dir = {light_x, light_y, light_z};
        vec3f light_pos = vec3f{-2.02812f, -0.55221f, -2.57514f} + light_dir * light_dist;
        light_dir = -light_dir;
        auto view = transform::look_at(light_pos, light_pos + light_dir, {0, 1, 0});
        auto proj = transform::orthographic(-5.f, 5.f, 5.f, -5.f, 0.1f, light_dist);
        return std::make_tuple(light_dir, proj * view);
    }

    void initialize() override {
        GL_EXPR(glEnable(GL_DEPTH_TEST));
        GL_EXPR(glClearColor(0, 0, 0, 0));
        GL_EXPR(glClearDepth(1.0));
        const std::string path = "asset/ReflectiveShadowMap/";

        quad_vao.initialize_handle();

        auto load_model = [&](const std::string& filename, const color3f& albedo, const mat4& mod){
            auto model = load_model_from_obj_file(filename);
            auto t = to_color3b(albedo);
            wzz::texture::image2d_t<wzz::math::color3b> tmp(2, 2, t);
            for(auto& mesh : model->meshes){
                auto& m = draw_meshes.emplace_back();
                m.model = mod;
                m.vao.initialize_handle();
                m.vbo.initialize_handle();
                m.ebo.initialize_handle();
                m.vbo.reinitialize_buffer_data(mesh.vertices.data(), mesh.vertices.size(), GL_STATIC_DRAW);
                m.ebo.reinitialize_buffer_data(mesh.indices.data(), mesh.indices.size(), GL_STATIC_DRAW);
                m.vao.bind_vertex_buffer_to_attrib(attrib_var_t<vec3f>(0), m.vbo, &Vertex::pos, 0);
                m.vao.bind_vertex_buffer_to_attrib(attrib_var_t<vec3f>(1), m.vbo, &Vertex::normal, 1);
                m.vao.bind_vertex_buffer_to_attrib(attrib_var_t<vec2f>(2), m.vbo, &Vertex::tex_coord, 2);
                m.vao.enable_attrib(attrib_var_t<vec3f>(0));
                m.vao.enable_attrib(attrib_var_t<vec3f>(1));
                m.vao.enable_attrib(attrib_var_t<vec2f>(2));
                m.vao.bind_index_buffer(m.ebo);
                m.albedo.initialize_handle();
                m.albedo.initialize_format_and_data(1, GL_RGB8, tmp);
            }
        };
        load_model(path + "corner.obj", {0.05f, 0.83f, 0.96f}, mat4::identity());
        //1 / 500
        load_model(path + "fullbody.obj", {0.7f, 0.7f, 0.7f}, transform::translate(0,4.6,0.2) * transform::rotate_x(PI) * transform::scale(0.01f, 0.01f, 0.01f));

        transform_buffer.initialize_handle();
        transform_buffer.reinitialize_buffer_data(nullptr, GL_STATIC_DRAW);

        rsm_resc.fbo.initialize_handle();
        rsm_resc.rbo.initialize_handle();
        rsm_resc.rbo.set_format(GL_DEPTH32F_STENCIL8, RSMWidth, RSMHeight);
        rsm_resc.fbo.attach(GL_DEPTH_STENCIL_ATTACHMENT, rsm_resc.rbo);
        rsm_resc.pos.initialize_handle();
        rsm_resc.pos.initialize_texture(1, GL_RGBA32F, RSMWidth, RSMHeight);
        rsm_resc.normal.initialize_handle();
        rsm_resc.normal.initialize_texture(1, GL_RGBA8, RSMWidth, RSMHeight);
        rsm_resc.flux.initialize_handle();
        rsm_resc.flux.initialize_texture(1, GL_RGBA32F, RSMWidth, RSMHeight);
        rsm_resc.fbo.attach(GL_COLOR_ATTACHMENT0, rsm_resc.pos);
        rsm_resc.fbo.attach(GL_COLOR_ATTACHMENT1, rsm_resc.normal);
        rsm_resc.fbo.attach(GL_COLOR_ATTACHMENT2, rsm_resc.flux);
        assert(rsm_resc.fbo.is_complete());

        auto [light_dir, vp] = getLight();
        light.light_direction = light_dir;
        light.light_radiance = light_color * light_intensity;
        light_buffer.initialize_handle();
        light_buffer.reinitialize_buffer_data(&light, GL_STATIC_DRAW);

        auto window_w = window->get_window_width();
        auto window_h = window->get_window_height();
        gbuffer_resc.fbo.initialize_handle();
        gbuffer_resc.rbo.initialize_handle();
        gbuffer_resc.rbo.set_format(GL_DEPTH32F_STENCIL8, window_w, window_h);
        gbuffer_resc.fbo.attach(GL_DEPTH_STENCIL_ATTACHMENT, gbuffer_resc.rbo);
        gbuffer_resc.gbuffer0.initialize_handle();
        gbuffer_resc.gbuffer0.initialize_texture(1, GL_RGBA32F, window_w, window_h);
        gbuffer_resc.gbuffer1.initialize_handle();
        gbuffer_resc.gbuffer1.initialize_texture(1, GL_RGBA32F, window_w, window_h);
        gbuffer_resc.fbo.attach(GL_COLOR_ATTACHMENT0, gbuffer_resc.gbuffer0);
        gbuffer_resc.fbo.attach(GL_COLOR_ATTACHMENT1, gbuffer_resc.gbuffer1);
        assert(gbuffer_resc.fbo.is_complete());

        low_res_guffer_resc.fbo.initialize_handle();
        low_res_guffer_resc.rbo.initialize_handle();
        low_res_guffer_resc.rbo.set_format(GL_DEPTH32F_STENCIL8, LowResW, LowResH);
        low_res_guffer_resc.fbo.attach(GL_DEPTH_STENCIL_ATTACHMENT, low_res_guffer_resc.rbo);
        low_res_guffer_resc.gbuffer0.initialize_handle();
        low_res_guffer_resc.gbuffer0.initialize_texture(1, GL_RGBA32F, LowResW, LowResH);
        low_res_guffer_resc.gbuffer1.initialize_handle();
        low_res_guffer_resc.gbuffer1.initialize_texture(1, GL_RGBA32F, LowResW, LowResW);
        lowres_indirect_tex.initialize_handle();
        lowres_indirect_tex.initialize_texture(1, GL_RGBA8, LowResW, LowResH);
        low_res_guffer_resc.fbo.attach(GL_COLOR_ATTACHMENT0, low_res_guffer_resc.gbuffer0);
        low_res_guffer_resc.fbo.attach(GL_COLOR_ATTACHMENT1, low_res_guffer_resc.gbuffer1);
        low_res_guffer_resc.fbo.attach(GL_COLOR_ATTACHMENT2, lowres_indirect_tex);
        assert(low_res_guffer_resc.fbo.is_complete());


        indirect_params.rsm_light_viewproj = vp;
        indirect_params_buffer.initialize_handle();
        indirect_params_buffer.reinitialize_buffer_data(&indirect_params, GL_STATIC_DRAW);


        rsm = program_t::build_from(
                shader_t<GL_VERTEX_SHADER>::from_file(path + "rsm.vert"),
                shader_t<GL_FRAGMENT_SHADER>::from_file(path + "rsm.frag")
                );
        gbuffer = program_t::build_from(
                shader_t<GL_VERTEX_SHADER>::from_file(path + "gbuffer.vert"),
                shader_t<GL_FRAGMENT_SHADER>::from_file(path + "gbuffer.frag")
                );
        lowres_indirect = program_t::build_from(
                shader_t<GL_VERTEX_SHADER>::from_file(path + "quad.vert"),
                shader_t<GL_FRAGMENT_SHADER>::from_file(path + "lowres_indirect.frag")
                );
        rsm_shading = program_t::build_from(
                shader_t<GL_VERTEX_SHADER>::from_file(path + "quad.vert"),
                shader_t<GL_FRAGMENT_SHADER>::from_file(path + "rsm_shading.frag")
                );

        // possion disk samples
        possion_disk_samples_buffer.initialize_handle();
        {
            std::default_random_engine rng{ std::random_device{}() };
            std::uniform_real_distribution<float> dis(0, 1);

            std::vector<cy::Point2f> candidateSamples;
            int sampleCount = indirect_params.poission_sample_count;
            for(int i = 0; i < sampleCount; ++i)
            {
                for(int j = 0; j < 4; ++j)
                {
                    const float x = dis(rng);
                    const float y = dis(rng);
                    candidateSamples.push_back({ x, y });
                }
            }

            std::vector<cy::Point2f> resultSamples(sampleCount);

            cy::WeightedSampleElimination<cy::Point2f, float, 2> wse;
            wse.SetTiling(true);
            wse.Eliminate(candidateSamples.data(), candidateSamples.size(),
                          resultSamples.data(), resultSamples.size());

            std::vector<vec4f> data(sampleCount);
            float sumWeights = 0;
            auto radius = indirect_params.indirect_sample_radius;
            for(int i = 0; i < sampleCount; ++i)
            {
                const auto &sam = resultSamples[i];

                vec4f point;
                point.x = radius * sam.x * std::cos(2 * 3.14159265f * sam.y);
                point.y = radius * sam.x * std::sin(2 * 3.14159265f * sam.y);
                point.z = sam.x * sam.x;

                sumWeights += point.z;
                data[i] = point;
            }

            float invSumWeights = 1 / (std::max)(sumWeights, 0.01f);
            for(auto &p : data)
                p.z *= invSumWeights;

            possion_disk_samples_buffer.initialize_buffer_data(data.data(), data.size(), GL_DYNAMIC_STORAGE_BIT);
        }

        camera.set_position({0,2,3});
        camera.set_direction(-wzz::math::PI_f * 0.5f, 0);
        camera.set_perspective(60.f, 0.1f, 100.f);
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

            bool update_light = false;
            update_light |= ImGui::SliderFloat("Light Y Degree", &light_y_degree, 0, 90);
            update_light |= ImGui::SliderFloat("Light X Degree", &light_x_degree, 0, 360);
            update_light |= ImGui::ColorEdit3("Light Radiance", &light_color.x);
            update_light |= ImGui::SliderFloat("Light Intensity", &light_intensity, 0, 10);
            if(update_light){
                auto [light_dir, vp] = getLight();
                light.light_direction = light_dir;
                light.light_radiance = light_color * light_intensity;
                light_buffer.set_buffer_data(&light);
                indirect_params_buffer.set_buffer_data(&indirect_params);
            }
            bool update_indirect = false;
            update_indirect |= ImGui::InputInt("PoissionDisk Sample Count", &indirect_params.poission_sample_count);
            indirect_params.poission_sample_count = std::min(indirect_params.poission_sample_count, 400);
            update_indirect |= ImGui::InputFloat("Indirect Sample Radius", &indirect_params.indirect_sample_radius);
            update_indirect |= ImGui::InputFloat("Light Proj World Area", &indirect_params.light_proj_world_area);
            update_indirect |= ImGui::Checkbox("Enable Indirect", &enable_indirect);
            update_indirect |= ImGui::Checkbox("Enable LowRes", &enable_lowres);
            if(update_indirect){
                indirect_params.enable_indirect = (enable_indirect ? 0b1 : 0) | (enable_lowres ? 0b10 : 0);
                enable_lowres = enable_indirect && enable_lowres;
                indirect_params_buffer.set_buffer_data(&indirect_params);
            }
        }
        auto camera_vp = camera.get_view_proj();
        transform.mvp = camera_vp;

        auto [light_dir, light_vp] = getLight();

        static GLenum render_targets[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};

        // rsm generate
        rsm_resc.fbo.bind();
        GL_EXPR(glDrawBuffers(3, render_targets));
        rsm_resc.fbo.clear_color_depth_buffer();
        GL_EXPR(glViewport(0, 0, RSMWidth, RSMHeight));
        rsm.bind();
        transform_buffer.bind(0);
        for(auto& draw_mesh : draw_meshes){
            transform.model = draw_mesh.model;
            transform.mvp = light_vp * draw_mesh.model;
            transform_buffer.set_buffer_data(&transform);
            draw_mesh.albedo.bind(0);
            draw_mesh.vao.bind();

            GL_EXPR(glDrawElements(GL_TRIANGLES, draw_mesh.ebo.index_count(), GL_UNSIGNED_INT, nullptr));

            draw_mesh.vao.unbind();
        }
        rsm.unbind();
        rsm_resc.fbo.unbind();

        // gbuffer pass
        gbuffer_resc.fbo.bind();
        GL_EXPR(glDrawBuffers(2, render_targets));
        // maybe not need
        gbuffer_resc.fbo.clear_color_depth_buffer();
        GL_EXPR(glViewport(0, 0, window->get_window_width(), window->get_window_height()));
        gbuffer.bind();
        transform_buffer.bind(0);
        for(auto& draw_mesh : draw_meshes){
            transform.model = draw_mesh.model;
            transform.mvp = camera_vp * draw_mesh.model;
            transform_buffer.set_buffer_data(&transform);
            draw_mesh.albedo.bind(0);
            draw_mesh.vao.bind();

            GL_EXPR(glDrawElements(GL_TRIANGLES, draw_mesh.ebo.index_count(), GL_UNSIGNED_INT, nullptr));

            draw_mesh.vao.unbind();
        }
        gbuffer.unbind();
        gbuffer_resc.fbo.unbind();


        // common texture binding
        gbuffer_resc.gbuffer0.bind(0);
        gbuffer_resc.gbuffer1.bind(1);


        rsm_resc.pos.bind(5);
        rsm_resc.normal.bind(6);
        rsm_resc.flux.bind(7);

        indirect_params_buffer.bind(0);
        possion_disk_samples_buffer.bind(0);

        // lowres pass
        low_res_guffer_resc.fbo.bind();
        GL_EXPR(glDrawBuffers(3, render_targets));
        low_res_guffer_resc.fbo.clear_color_depth_buffer();
        GL_EXPR(glViewport(0, 0, LowResW, LowResH));
        lowres_indirect.bind();
        quad_vao.bind();
        GL_EXPR(glDepthFunc(GL_LEQUAL));
        GL_EXPR(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));
        GL_EXPR(glDepthFunc(GL_LESS));
        lowres_indirect.unbind();
        quad_vao.unbind();
        low_res_guffer_resc.fbo.unbind();


        // rsm shading pass
        low_res_guffer_resc.gbuffer0.bind(2);
        low_res_guffer_resc.gbuffer1.bind(3);
        lowres_indirect_tex.bind(4);
        light_buffer.bind(1);

        framebuffer_t::bind_to_default();
        framebuffer_t::clear_color_depth_buffer();
        GL_EXPR(glViewport(0, 0, window->get_window_width(), window->get_window_height()));
        rsm_shading.bind();
        quad_vao.bind();
        GL_EXPR(glDepthFunc(GL_LEQUAL));
        GL_EXPR(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));
        GL_EXPR(glDepthFunc(GL_LESS));
        quad_vao.unbind();
        rsm_shading.unbind();

        ImGui::End();
    }



    void destroy() override {

    }


private:
    bool vsync = true;

    vertex_array_t quad_vao;

//    struct Vertex{
//        vec3 pos;
//        vec3 normal;
//        vec2 uv;
//    };
    using Vertex = wzz::model::vertex_t;
    struct DrawMesh{
        mat4 model;
        vertex_array_t vao;
        vertex_buffer_t<Vertex> vbo;
        index_buffer_t<uint32_t> ebo;
        texture2d_t albedo;
    };

    std::vector<DrawMesh> draw_meshes;

    struct Transform{
        mat4 model;
        mat4 mvp;
    }transform;
    std140_uniform_block_buffer_t<Transform> transform_buffer;

    static constexpr const int RSMWidth = 512;
    static constexpr const int RSMHeight = 512;
    static constexpr const int LowResW = 256;
    static constexpr const int LowResH = 256;
    struct RSMResc{
        framebuffer_t fbo;
        renderbuffer_t rbo;
        texture2d_t pos;
        texture2d_t normal;
        texture2d_t flux;
    }rsm_resc;

    float light_x_degree = 45.f;
    float light_y_degree = 60.f;
    vec3f light_color = {1.f,1.f,1.f};
    float light_intensity = 1.f;
    struct Light{
        vec3 light_direction; float pad0;
        vec3 light_radiance;  float pad1;
    }light;
    std140_uniform_block_buffer_t<Light> light_buffer;

    struct GBuffer{
        framebuffer_t fbo;
        renderbuffer_t rbo;
        texture2d_t gbuffer0;
        texture2d_t gbuffer1;
    };
    GBuffer gbuffer_resc;
    GBuffer low_res_guffer_resc;

    texture2d_t lowres_indirect_tex;

    bool enable_indirect = false;
    bool enable_lowres = false;
    struct IndirectParams{
        mat4 rsm_light_viewproj;
        int poission_sample_count = 400;
        float indirect_sample_radius = 0.2f;
        float light_proj_world_area = 144;
        int enable_indirect = 0;
    }indirect_params;
    std140_uniform_block_buffer_t<IndirectParams> indirect_params_buffer;


    storage_buffer_t<vec4> possion_disk_samples_buffer;

    program_t rsm;
    program_t gbuffer;
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