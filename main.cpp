/**
* Author: Connor Chavez
* Assignment: Lunar Lander
* Date due: 2025-3-15, 11:59pm
* I pledge that I have completed this assignment without
* collaborating with anyone else, in conformance with the
* NYU School of Engineering Policies and Procedures on
* Academic Misconduct.
**/

#define GL_SILENCE_DEPRECATION
#define STB_IMAGE_IMPLEMENTATION
#define LOG(argument) std::cout << argument << '\n'
#define GL_GLEXT_PROTOTYPES 1
#define FIXED_TIMESTEP 0.0166666f
#define PLATFORM_COUNT 13

#ifdef _WINDOWS
#include <GL/glew.h>
#endif

#include <SDL.h>
#include <SDL_opengl.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"
#include "cmath"
#include <ctime>
#include <vector>
#include "Entity.h"

// ––––– STRUCTS AND ENUMS ––––– //
struct GameState
{
    Entity* player;
    Entity* platforms;
    Entity* background;
    Entity* points;
};

// ––––– CONSTANTS ––––– //
const int WINDOW_WIDTH = 640*1.2f,
WINDOW_HEIGHT = 480;

const float BG_RED = 0.1922f,
BG_BLUE = 0.549f,
BG_GREEN = 0.9059f,
BG_OPACITY = 1.0f;

const int VIEWPORT_X = 0,
VIEWPORT_Y = 0,
VIEWPORT_WIDTH = WINDOW_WIDTH,
VIEWPORT_HEIGHT = WINDOW_HEIGHT;

const char V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
F_SHADER_PATH[] = "shaders/fragment_textured.glsl";

const float MILLISECONDS_IN_SECOND = 1000.0;
const char SPRITESHEET_FILEPATH[] = "assets/SeamothSprites.png";
const char PLATFORM_FILEPATH[] = "assets/platformPack_tile027.png";
const char DANGER_FILEPATH[] = "assets/DangerHorizontal.png";
const char REAPER_FILEPATH[] = "assets/ReaperLeviathan.png";
const char BACKGROUND_FILEPATH[] = "assets/SubnauticaBackground4.png";
const char POINTS_FILEPATH[] = "assets/Points.png";
const char FONT_FILEPATH[] = "assets/font1.png";


constexpr int FONTBANK_SIZE = 16;

const int NUMBER_OF_TEXTURES = 1;
const GLint LEVEL_OF_DETAIL = 0;
const GLint TEXTURE_BORDER = 0;

// ––––– GLOBAL VARIABLES ––––– //
GameState g_state;

SDL_Window* g_display_window;
bool g_game_is_running = true;

ShaderProgram g_program;
glm::mat4 g_view_matrix, g_projection_matrix;

float g_previous_ticks = 0.0f;
float g_accumulator = 0.0f;

float gravity = -0.09f;
float drag = 0.001f;
float horizontal_acceleration = 20.0f;
float acceleration_rate = 0.01f;
float vertical_acceleration = 0.2f;
int fuel = 100000;
int fuel_consumption = 1;

// ––––– GENERAL FUNCTIONS ––––– //
GLuint load_texture(const char* filepath)
{
    int width, height, number_of_components;
    unsigned char* image = stbi_load(filepath, &width, &height, &number_of_components, STBI_rgb_alpha);

    if (image == NULL)
    {
        LOG("Unable to load image. Make sure the path is correct.");
        assert(false);
    }

    GLuint textureID;
    glGenTextures(NUMBER_OF_TEXTURES, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, LEVEL_OF_DETAIL, GL_RGBA, width, height, TEXTURE_BORDER, GL_RGBA, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);


    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(image);

    return textureID;
}

void draw_text(ShaderProgram* shader_program, GLuint font_texture_id, std::string text,
    float font_size, float spacing, glm::vec3 position)
{
    // Scale the size of the fontbank in the UV-plane
    // We will use this for spacing and positioning
    float width = 1.0f / FONTBANK_SIZE;
    float height = 1.0f / FONTBANK_SIZE;

    // Instead of having a single pair of arrays, we'll have a series of pairs—one for
    // each character. Don't forget to include <vector>!
    std::vector<float> vertices;
    std::vector<float> texture_coordinates;

    // For every character...
    for (int i = 0; i < text.size(); i++) {
        // 1. Get their index in the spritesheet, as well as their offset (i.e. their
        //    position relative to the whole sentence)
        int spritesheet_index = (int)text[i];  // ascii value of character
        float offset = (font_size + spacing) * i;

        // 2. Using the spritesheet index, we can calculate our U- and V-coordinates
        float u_coordinate = (float)(spritesheet_index % FONTBANK_SIZE) / FONTBANK_SIZE;
        float v_coordinate = (float)(spritesheet_index / FONTBANK_SIZE) / FONTBANK_SIZE;

        // 3. Inset the current pair in both vectors
        vertices.insert(vertices.end(), {
            offset + (-0.5f * font_size), 0.5f * font_size,
            offset + (-0.5f * font_size), -0.5f * font_size,
            offset + (0.5f * font_size), 0.5f * font_size,
            offset + (0.5f * font_size), -0.5f * font_size,
            offset + (0.5f * font_size), 0.5f * font_size,
            offset + (-0.5f * font_size), -0.5f * font_size,
            });

        texture_coordinates.insert(texture_coordinates.end(), {
            u_coordinate, v_coordinate,
            u_coordinate, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate + width, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate, v_coordinate + height,
            });
    }

    // 4. And render all of them using the pairs
    glm::mat4 model_matrix = glm::mat4(1.0f);
    model_matrix = glm::translate(model_matrix, position);

    shader_program->set_model_matrix(model_matrix);
    glUseProgram(shader_program->get_program_id());

    glVertexAttribPointer(shader_program->get_position_attribute(), 2, GL_FLOAT, false, 0,
        vertices.data());
    glEnableVertexAttribArray(shader_program->get_position_attribute());

    glVertexAttribPointer(shader_program->get_tex_coordinate_attribute(), 2, GL_FLOAT,
        false, 0, texture_coordinates.data());
    glEnableVertexAttribArray(shader_program->get_tex_coordinate_attribute());

    glBindTexture(GL_TEXTURE_2D, font_texture_id);
    glDrawArrays(GL_TRIANGLES, 0, (int)(text.size() * 6));

    glDisableVertexAttribArray(shader_program->get_position_attribute());
    glDisableVertexAttribArray(shader_program->get_tex_coordinate_attribute());
}

GLuint g_font_texture_id;

void initialise()
{
    SDL_Init(SDL_INIT_VIDEO);
    g_display_window = SDL_CreateWindow("Safe(?) Shallows Seamoth",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL);

    SDL_GLContext context = SDL_GL_CreateContext(g_display_window);
    SDL_GL_MakeCurrent(g_display_window, context);

#ifdef _WINDOWS
    glewInit();
#endif

    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    g_program.load(V_SHADER_PATH, F_SHADER_PATH);

    g_view_matrix = glm::mat4(1.0f);
    g_projection_matrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);

    g_program.set_projection_matrix(g_projection_matrix);
    g_program.set_view_matrix(g_view_matrix);

    glUseProgram(g_program.get_program_id());

    glClearColor(BG_RED, BG_BLUE, BG_GREEN, BG_OPACITY);
    // ––––– BACKGROUND ––––– //
    GLuint background_texture_id = load_texture(BACKGROUND_FILEPATH);
    GLuint points_id = load_texture(POINTS_FILEPATH);
    g_font_texture_id = load_texture(FONT_FILEPATH);
    
    g_state.background = new Entity();
    g_state.background->set_position(glm::vec3(0.0f));
    g_state.background->m_texture_id = background_texture_id;
    g_state.background->set_scale(glm::vec3(10.0f, 10.0f, 0.0f));
    g_state.background->update(0.0f, NULL, 0);

    g_state.points = new Entity();
    g_state.points->set_position(glm::vec3(0.0f));
    g_state.points->m_texture_id = points_id;
    g_state.points->set_scale(glm::vec3(10.0f, 10.0f, 0.0f));
    g_state.points->update(0.0f, NULL, 0);


    
    // ––––– PLATFORMS ––––– //
    GLuint platform_texture_id = load_texture(PLATFORM_FILEPATH);
    GLuint danger_texture_id = load_texture(DANGER_FILEPATH);
    GLuint reaper_texture_id = load_texture(REAPER_FILEPATH);


    g_state.platforms = new Entity[PLATFORM_COUNT];

    for (int i = 0; i < 5; i++) {
        g_state.platforms[i].object_wins();
    }

    for (int i = 5; i < PLATFORM_COUNT; i++) {
        g_state.platforms[i].object_loses();
        if (i >5) g_state.platforms[i].m_texture_id = danger_texture_id;
    }

    //x1 point platform
    g_state.platforms[0].m_texture_id = platform_texture_id;
    g_state.platforms[0].set_position(glm::vec3(-4.0f, -0.9f, 0.0f));
    g_state.platforms[0].set_dimensions(glm::vec3(0.8f, 1.0f, 0.0f));
    g_state.platforms[0].update(0.0f, NULL, 0);

    
    //x2 point platform
    g_state.platforms[1].m_texture_id = platform_texture_id;
    g_state.platforms[1].set_position(glm::vec3(-0.5f, 0.6f, 0.0f));
    g_state.platforms[1].set_dimensions(glm::vec3(0.6f, 1.0f, 0.0f));
    g_state.platforms[1].update(0.0f, NULL, 0);

    

    //x3 point platform
    g_state.platforms[2].m_texture_id = platform_texture_id;
    g_state.platforms[2].set_position(glm::vec3(0.9f, -0.8f, 0.0f));
    g_state.platforms[2].set_dimensions(glm::vec3(0.7f, 1.0f, 0.0f));
    g_state.platforms[2].update(0.0f, NULL, 0);
    
    //x5 point platform
    g_state.platforms[4].m_texture_id = platform_texture_id;
    g_state.platforms[4].set_position(glm::vec3(2.1f, -2.5f, 0.0f));
    g_state.platforms[4].set_dimensions(glm::vec3(0.5f, 1.0f, 0.0f));
    g_state.platforms[4].update(0.0f, NULL, 0);

    //x4 point platform
    g_state.platforms[3].m_texture_id = platform_texture_id;
    g_state.platforms[3].set_position(glm::vec3(-1.7f, -1.0f, 0.0f));
    g_state.platforms[3].set_dimensions(glm::vec3(0.8f, 1.0f, 0.0f));
    g_state.platforms[3].update(0.0f, NULL, 0);

    //Reaper Leviathan
    g_state.platforms[5].m_texture_id = reaper_texture_id;
    g_state.platforms[5].set_position(glm::vec3(3.0f, 2.0f, 0.0f));
    g_state.platforms[5].set_dimensions(glm::vec3(1.5f, 1.0f, 0.0f));

    g_state.platforms[5].m_speed = 1.0f;
    g_state.platforms[5].update(0.0f, NULL, 0);

    //Tube left
    
    g_state.platforms[6].set_position(glm::vec3(-2.4f, -0.5f, 0.0f));
    g_state.platforms[6].set_dimensions(glm::vec3(0.5f, 2.5f, 0.0f));
    g_state.platforms[6].update(0.0f, NULL, 0);
    
    //Left of 1x
    g_state.platforms[7].set_position(glm::vec3(-4.7f, -0.5f, 0.0f));
    g_state.platforms[7].set_dimensions(glm::vec3(0.5f, 1.0f, 0.0f));
    g_state.platforms[7].update(0.0f, NULL, 0);

    //Right of 1x
    g_state.platforms[8].set_position(glm::vec3(-3.1f, -1.2f, 0.0f));
    g_state.platforms[8].set_dimensions(glm::vec3(0.8f, 1.0f, 0.0f));
    g_state.platforms[8].update(0.0f, NULL, 0);
    

    //Tube right
    g_state.platforms[9].set_position(glm::vec3(-0.6f, -0.5f, 0.0f));
    g_state.platforms[9].set_dimensions(glm::vec3(1.3f, 2.5f, 0.0f));
    g_state.platforms[9].update(0.0f, NULL, 0);

    //left of 3x
    g_state.platforms[10].set_position(glm::vec3(0.3f, -0.9f, 0.0f));
    g_state.platforms[10].set_dimensions(glm::vec3(0.6f, 1.0f, 0.0f));
    g_state.platforms[10].update(0.0f, NULL, 0);

    //right of 3x
    g_state.platforms[11].set_position(glm::vec3(1.5f, -1.1f, 0.0f));
    g_state.platforms[11].set_dimensions(glm::vec3(0.6f, 2.5f, 0.0f));
    g_state.platforms[11].update(0.0f, NULL, 0);

    //right of 5x
    g_state.platforms[12].set_position(glm::vec3(3.7f, -1.3f, 0.0f));
    g_state.platforms[12].set_dimensions(glm::vec3(2.5f, 1.4f, 0.0f));
    g_state.platforms[12].update(0.0f, NULL, 0);





    // ––––– PLAYER (GEORGE) ––––– //
    // Existing
    g_state.player = new Entity();
    g_state.player->set_position(glm::vec3(-3.0f,2.0f,0.0f));
    g_state.player->set_movement(glm::vec3(0.0f));
    g_state.player->set_dimensions(glm::vec3(0.6f, 0.8f, 0.0f));
    g_state.player->m_speed = 1.0f;
    g_state.player->set_acceleration(glm::vec3(0.0f, gravity, 0.0f));
    g_state.player->m_texture_id = load_texture(SPRITESHEET_FILEPATH);

    // Walking
    g_state.player->m_walking[g_state.player->LEFT] = new int[4] { 0 };
    g_state.player->m_walking[g_state.player->RIGHT] = new int[4] { 1 };

    g_state.player->m_animation_indices = g_state.player->m_walking[g_state.player->LEFT];  // start George looking left
    g_state.player->m_animation_frames = 1;
    g_state.player->m_animation_index = 0;
    g_state.player->m_animation_time = 0.0f;
    g_state.player->m_animation_cols = 2;
    g_state.player->m_animation_rows = 1;

    // Jumping
    g_state.player->m_jumping_power = 3.0f;

    

    // ––––– GENERAL ––––– //
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void process_input()
{
    g_state.player->set_movement(glm::vec3(0.0f));

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
            // End game
        case SDL_QUIT:
        case SDL_WINDOWEVENT_CLOSE:
            g_game_is_running = false;
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_q:
                // Quit the game with a keystroke
                g_game_is_running = false;
                break;
            default:
                break;
            }

        default:
            break;
        }
    }

    const Uint8* key_state = SDL_GetKeyboardState(NULL);

    if (!g_state.player->has_object_lost() && !g_state.player->has_object_won()) {


        if (key_state[SDL_SCANCODE_LEFT] && fuel > 0)
        {
            g_state.player->player_accelerate_left(acceleration_rate,horizontal_acceleration);
            g_state.player->m_animation_indices = g_state.player->m_walking[g_state.player->LEFT];
            fuel -= fuel_consumption;
        }
        else if (key_state[SDL_SCANCODE_RIGHT] && fuel > 0)
        {
            g_state.player->player_accelerate_right(acceleration_rate, horizontal_acceleration);
            g_state.player->m_animation_indices = g_state.player->m_walking[g_state.player->RIGHT];
            fuel -= fuel_consumption;
        }
        else if (key_state[SDL_SCANCODE_UP] && fuel > 0)
        {
            g_state.player->set_acceleration_y(vertical_acceleration);
            fuel -= fuel_consumption;
        }
        else if (g_state.player->get_acceleration().x != 0) {
            g_state.player->player_drag(drag);
            g_state.player->set_acceleration_y(gravity);
        }
    }
    


    if (glm::length(g_state.player->m_movement) > 1.0f)
    {
        g_state.player->m_movement = glm::normalize(g_state.player->m_movement);
    }
}

float ANGLE = 0.0f;
glm::vec3 reaper_movement;

void update()
{
    float ticks = (float)SDL_GetTicks() / MILLISECONDS_IN_SECOND;
    float delta_time = ticks - g_previous_ticks;
    g_previous_ticks = ticks;

    delta_time += g_accumulator;

    if (delta_time < FIXED_TIMESTEP)
    {
        g_accumulator = delta_time;
        return;
    }


    while (delta_time >= FIXED_TIMESTEP && !g_state.player->has_object_won() && !g_state.player->has_object_lost())
    {
        //Reaper movement
        g_state.platforms[5].update(FIXED_TIMESTEP, NULL, 0);
        g_state.platforms[5].set_position(glm::vec3(cos(ANGLE/2.0f) + 3.0f, sin(ANGLE) + 2.0f, 0.0f));
        ANGLE += 1.0f * FIXED_TIMESTEP;

        g_state.player->update(FIXED_TIMESTEP, g_state.platforms, PLATFORM_COUNT);
        if (g_state.player->get_position().x < -4.8 || g_state.player->get_position().x > 4.8) {
            g_state.player->object_loses();
        }
        

        delta_time -= FIXED_TIMESTEP;
    }

    g_accumulator = delta_time;

   
}

float TIMER = 0;

void render()
{
    glClear(GL_COLOR_BUFFER_BIT);

    g_state.background->render(&g_program);

    //Reaper
    g_state.platforms[5].render(&g_program);

    //Makes danger signs and point values blink
    if (10000 - TIMER >= 5000 || g_state.player->has_object_lost() || g_state.player->has_object_won()) {
        g_state.points->render(&g_program);

        for (int i = 6; i < PLATFORM_COUNT; i++) g_state.platforms[i].render(&g_program);
    }
    else if (10000 - TIMER == 0) {
        TIMER = 0;
    }
    TIMER += 1;
    

    g_state.player->render(&g_program);
    

    std::string fuel_ui = "Fuel: ";
    std::string fuel_string = std::to_string(fuel);
    fuel_ui += fuel_string;

    draw_text(&g_program, g_font_texture_id, fuel_ui, 0.5f, 0.005f,
        glm::vec3(-4.5f, 3.5f, 0.0f));

    if (g_state.player->has_object_won()) {
        draw_text(&g_program, g_font_texture_id, "Seamoth Parked", 0.5f, 0.005f,
            glm::vec3(-3.5f, 1.5f, 0.0f));
    }
    else if (g_state.player->has_object_lost()) {
        draw_text(&g_program, g_font_texture_id, "Seamoth Crashed", 0.5f, 0.005f,
            glm::vec3(-3.5f, 1.5f, 0.0f));
    }


    SDL_GL_SwapWindow(g_display_window);
}

void shutdown()
{
    SDL_Quit();

    delete[] g_state.platforms;
    delete g_state.player;
}

// ––––– GAME LOOP ––––– //
int main(int argc, char* argv[])
{
    initialise();

    while (g_game_is_running)
    {
        process_input();
        update();
        render();
    }

    shutdown();
    return 0;
}