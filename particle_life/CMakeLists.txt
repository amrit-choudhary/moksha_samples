# Particle Life: a CPU simulation load test. Particles of six types attract and repel each other by random rules and
# glow where they crowd (additive instanced sprites); - and + change the particle count. The texture is generated.

moksha_add_game(Sample_ParticleLife
    SOURCES
        game_main.cpp
        game/game_particle_life.cpp
        game/particle_life_sim.cpp
        scene/scene_particle_life.cpp
        scene/scene_ui_particle_life.cpp
    RES_LIST resources/res_list.json
    HEADLESS
    DISPLAY_NAME "Particle Life"
    ORIENTATION LANDSCAPE)
