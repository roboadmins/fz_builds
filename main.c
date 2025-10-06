#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/dialog_ex.h>  // For simple confirm
#include <subghz/subghz.h>
#include <subghz/subghz_file_encoder_worker.h>
#include <storage/storage.h>
#include <lib/toolbox/path.h>
#include <toolbox/level_duration.h>

typedef enum {
    SubGHzGhostViewStart,
    SubGHzGhostViewReplay,
} SubGHzGhostView;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    DialogEx* dialog;
    SubGhz* subghz;
    FuriString* file_path;
    Storage* storage;
    LevelDuration signal_duration;  // For detect
} SubGHzGhostApp;

static void subghz_ghost_dialog_callback(DialogExResult result, void* context) {
    SubGHzGhostApp* app = context;
    if(result == DialogExResultOk) {
        // Replay
        subghz_tx_rx_worker_start(app->subghz->txrx->worker, SubGhzRadioChannelTypeRAW, &app->signal_duration);
        furi_delay_ms(1000);  // Send burst
        subghz_tx_rx_worker_stop(app->subghz->txrx->worker);
        // For brute: Would need dict load—stub for now
    }
    view_dispatcher_stop(app->view_dispatcher);
}

static uint32_t subghz_ghost_exit(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

int32_t subghz_ghost_main(void* p) {
    UNUSED(p);
    SubGHzGhostApp* app = malloc(sizeof(SubGHzGhostApp));

    // Init
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->subghz = furi_record_open(RECORD_SUBGHZ);
    app->dialog = dialog_ex_alloc();
    app->file_path = furi_string_alloc();
    app->storage = furi_record_open(RECORD_STORAGE);

    // Quick scan (RAW read at 433.92MHz common)
    subghz_manual_off(app->subghz);
    subghz_rx(app->subghz, 433920000);  // Freq in Hz
    furi_delay_ms(5000);  // Listen 5s—hold near target
    level_duration_get(app->subghz->txrx->worker->level_duration, &app->signal_duration);  // Capture

    // Save .sub (stub RAW encode—real would use worker)
    storage_common_copy(app->storage, "/int/.subghz/ghost.sub", "/ext/subghz/ghost_%d.sub", furi_get_tick());  // Timestamp name
    furi_string_printf(app->file_path, "/ext/subghz/ghost_%ld.sub", furi_get_tick());

    // Dialog for replay confirm
    dialog_ex_reset(app->dialog);
    dialog_ex_set_text(app->dialog, "Ghost snagged!\nReplay now?");
    dialog_ex_set_context(app->dialog, app);
    dialog_ex_set_result_callback(app->dialog, subghz_ghost_dialog_callback);
    dialog_ex_set_left_button_text(app->dialog, "Brute");
    dialog_ex_set_right_button_text(app->dialog, "Replay");
    dialog_ex_set_icon(app->dialog, 0, 16, &I_channel_up_10x8);

    view_dispatcher_add_view(
        app->view_dispatcher,
        SubGHzGhostViewReplay,
        dialog_ex_get_view(app->dialog));

    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher,
        subghz_ghost_exit);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubGHzGhostViewReplay);

    // Run
    view_dispatcher_run(app->view_dispatcher);

    // Cleanup
    view_dispatcher_remove_view(app->view_dispatcher, SubGHzGhostViewReplay);
    dialog_ex_free(app->dialog);
    furi_string_free(app->file_path);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_SUBGHZ);
    furi_record_close(RECORD_STORAGE);
    view_dispatcher_free(app->view_dispatcher);
    free(app);

    return 0;
}
