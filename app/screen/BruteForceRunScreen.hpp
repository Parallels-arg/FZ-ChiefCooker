#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/elements.h>

#include "lib/HandlerContext.hpp"
#include "lib/String.hpp"
#include "app/pager/PagerReceiver.hpp"
#include "lib/hardware/subghz/SubGhzModule.hpp"
#include "lib/ui/view/UiView.hpp"
#include "lib/FlipperDolphin.hpp"
#include "lib/ui/UiManager.hpp"
#include "app/AppConfig.hpp"
#include "app/pager/PagerSerializer.hpp"

class BruteForceRunScreen : public UiView {
private:
    View* view;
    AppConfig* config;
    SubGhzModule* subghz;
    PagerReceiver* receiver;

    uint8_t speedIndex;
    bool isTryAll;
    uint8_t currentDecoderIdx;
    uint8_t initialDecoderIdx;

    uint16_t stationMin;
    uint16_t stationMax;
    uint16_t pagerMin;
    uint16_t pagerMax;

    uint16_t currentStation;
    uint16_t currentPager;

    uint8_t repeats;
    uint32_t delayMs;

    bool isPaused;
    bool isTransmitting;
    bool isHolding;
    bool isFinished;

    FuriTimer* delayTimer = NULL;

    String encStr;
    String detailsStr;
    String statusStr;

    static void drawCallback(Canvas* canvas, void* model) {
        BruteForceRunScreen* instance = (BruteForceRunScreen*)((UiVIewPointerViewModel*)model)->uiVIew;
        instance->draw(canvas);
    }

    static bool inputCallback(InputEvent* event, void* context) {
        BruteForceRunScreen* instance = (BruteForceRunScreen*)context;
        return instance->inputHandler(event);
    }

    static void delayTimerCallback(void* context) {
        BruteForceRunScreen* instance = (BruteForceRunScreen*)context;
        instance->onDelayTimerExpired();
    }

public:
    BruteForceRunScreen(
        AppConfig* config,
        SubGhzModule* subghz,
        PagerReceiver* receiver,
        uint8_t speedIndex,
        bool isTryAll,
        uint8_t selectedDecoderIdx,
        uint16_t stationMin,
        uint16_t stationMax,
        uint16_t pagerMin,
        uint16_t pagerMax
    ) {
        this->config = config;
        this->subghz = subghz;
        this->receiver = receiver;
        this->speedIndex = speedIndex;
        this->isTryAll = isTryAll;
        this->initialDecoderIdx = selectedDecoderIdx;
        this->currentDecoderIdx = isTryAll ? 0 : selectedDecoderIdx;

        this->pagerMin = pagerMin;
        this->pagerMax = pagerMax;
        this->currentPager = pagerMin;

        if(isTryAll) {
            this->stationMin = 0;
            this->stationMax = receiver->decoders[currentDecoderIdx]->GetMaxStation();
        } else {
            this->stationMin = stationMin;
            this->stationMax = stationMax;
        }
        this->currentStation = this->stationMin;

        // Speed configurations
        switch(speedIndex) {
        case 0: // Max
            repeats = 2;
            delayMs = 0;
            break;
        case 1: // Normal
            repeats = 5;
            delayMs = 80;
            break;
        case 2: // Slow
            repeats = 10;
            delayMs = 250;
            break;
        default:
            repeats = 2;
            delayMs = 0;
            break;
        }

        this->isPaused = false;
        this->isTransmitting = false;
        this->isHolding = false;
        this->isFinished = false;

        delayTimer = furi_timer_alloc(delayTimerCallback, FuriTimerTypeOnce, this);

        view = view_alloc();
        view_set_context(view, this);
        view_set_draw_callback(view, drawCallback);
        view_set_input_callback(view, inputCallback);
        view_allocate_model(view, ViewModelTypeLockFree, sizeof(BruteForceRunScreen*));
        with_view_model_cpp(view, UiVIewPointerViewModel*, model, model->uiVIew = this;, false);

        subghz->SetTransmitCompleteHandler(HANDLER(&BruteForceRunScreen::onTxComplete));

        sendCurrentCandidate();
    }

    View* GetNativeView() {
        return view;
    }

    ~BruteForceRunScreen() {
        if(delayTimer != NULL) {
            furi_timer_stop(delayTimer);
            furi_timer_free(delayTimer);
            delayTimer = NULL;
        }

        if(view != NULL) {
            OnDestory();
            view_free_model(view);
            view_free(view);
            view = NULL;
        }
        subghz->SetTransmitCompleteHandler(NULL);
        subghz->PutToIdle();
    }

private:
    void sendCurrentCandidate() {
        if(isFinished) return;

        PagerDecoder* decoder = receiver->decoders[currentDecoderIdx];
        PagerProtocol* protocol = receiver->protocols[0]; // Princeton protocol default

        uint32_t data = 0;
        data = decoder->SetStation(data, currentStation);
        data = decoder->SetPager(data, currentPager);
        data = decoder->SetAction(data, RING);

        subghz->Transmit(protocol->CreatePayload(data, protocol->GetMaxTE(), repeats), config->Frequency);
        FlipperDolphin::Deed(DolphinDeedSubGhzSend);

        isTransmitting = true;
        Refresh();
    }

    void onTxComplete() {
        isTransmitting = false;

        if(isHolding) {
            // User is actively holding button: send again continuously
            sendCurrentCandidate();
            return;
        }

        if(!isPaused && !isFinished) {
            if(delayMs > 0) {
                subghz->PutToIdle(); // Turn off LED and radio between intervals
                furi_timer_start(delayTimer, furi_kernel_get_tick_frequency() * delayMs / 1000);
            } else {
                advanceToNext();
                sendCurrentCandidate();
            }
        } else {
            subghz->PutToIdle(); // Turn off LED and radio immediately when paused or finished
            Refresh();
        }
    }

    void onDelayTimerExpired() {
        if(!isPaused && !isFinished && !isTransmitting) {
            advanceToNext();
            sendCurrentCandidate();
        }
    }

    void advanceToNext() {
        currentPager++;
        if(currentPager > pagerMax) {
            currentPager = pagerMin;
            currentStation++;
            if(currentStation > stationMax) {
                if(isTryAll && (currentDecoderIdx + 1 < receiver->decodersCount)) {
                    currentDecoderIdx++;
                    stationMin = 0;
                    stationMax = receiver->decoders[currentDecoderIdx]->GetMaxStation();
                    currentStation = 0;
                    currentPager = pagerMin;
                } else {
                    isFinished = true;
                    isPaused = true;
                    currentStation = stationMax;
                    currentPager = pagerMax;
                }
            }
        }
    }

    void stepPrevious() {
        if(currentPager > pagerMin) {
            currentPager--;
        } else {
            currentPager = pagerMax;
            if(currentStation > stationMin) {
                currentStation--;
            } else {
                if(isTryAll && currentDecoderIdx > 0) {
                    currentDecoderIdx--;
                    stationMin = 0;
                    stationMax = receiver->decoders[currentDecoderIdx]->GetMaxStation();
                    currentStation = stationMax;
                    currentPager = pagerMax;
                } else {
                    currentStation = stationMin;
                    currentPager = pagerMin;
                }
            }
        }
    }

    void stepNextManual() {
        advanceToNext();
        if(isFinished) {
            isFinished = false; // allow looping in manual step
        }
    }

    void draw(Canvas* canvas) {
        canvas_clear(canvas);
        canvas_set_color(canvas, ColorBlack);

        // 1. Header Box (y=0..12)
        if(isPaused) {
            canvas_draw_box(canvas, 0, 0, 128, 12);
            canvas_set_color(canvas, ColorWhite);
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignCenter, isFinished ? "COMPLETED" : "PAUSED");
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_set_font(canvas, FontPrimary);
            const char* speedName = (speedIndex == 0) ? "MAX" : (speedIndex == 1 ? "NORM" : "SLOW");
            statusStr.format("SWEEPING [%s]", speedName);
            canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignCenter, statusStr.cstr());
        }

        // 2. Encoding line (y=23)
        canvas_set_font(canvas, FontSecondary);
        PagerDecoder* decoder = receiver->decoders[currentDecoderIdx];
        if(isTryAll) {
            encStr.format("Enc: %s (%d/%d)", decoder->GetShortName(), currentDecoderIdx + 1, receiver->decodersCount);
        } else {
            encStr.format("Enc: %s", decoder->GetShortName());
        }
        canvas_draw_str(canvas, 4, 23, encStr.cstr());

        // 3. Station and Pager line (y=34)
        detailsStr.format("St: %d/%d  Pg: %d", currentStation, stationMax, currentPager);
        canvas_draw_str(canvas, 4, 34, detailsStr.cstr());

        // 4. Progress bar (y=40..46)
        float progress = 0.0f;
        if(stationMax > stationMin) {
            float stationFraction = (float)(currentStation - stationMin) / (stationMax - stationMin);
            if(isTryAll) {
                progress = ((float)currentDecoderIdx + stationFraction) / receiver->decodersCount;
            } else {
                progress = stationFraction;
            }
        }
        if(progress > 1.0f) progress = 1.0f;
        elements_progress_bar(canvas, 4, 39, 120, progress);

        // 5. Bottom Navigation Hints (y=57, concise and fitting screen)
        if(isPaused) {
            if(isTransmitting) {
                canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignCenter, "* TX... *");
            } else {
                canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignCenter, "OK:Play  < >:Step  Hold:Tx");
            }
        } else {
            canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignCenter, "OK:Pause  < >:Step");
        }
    }

    bool inputHandler(InputEvent* event) {
        if(event->type == InputTypeShort) {
            if(event->key == InputKeyOk) {
                isPaused = !isPaused;
                if(isFinished) isFinished = false;

                if(isPaused) {
                    if(delayTimer != NULL) furi_timer_stop(delayTimer);
                    subghz->PutToIdle();
                    isTransmitting = false;
                } else {
                    isTransmitting = false;
                    advanceToNext();
                    sendCurrentCandidate();
                }
                Refresh();
                return true;
            } else if(event->key == InputKeyLeft) {
                isPaused = true;
                if(delayTimer != NULL) furi_timer_stop(delayTimer);
                subghz->PutToIdle();
                isTransmitting = false;
                stepPrevious();
                sendCurrentCandidate();
                Refresh();
                return true;
            } else if(event->key == InputKeyRight) {
                isPaused = true;
                if(delayTimer != NULL) furi_timer_stop(delayTimer);
                subghz->PutToIdle();
                isTransmitting = false;
                stepNextManual();
                sendCurrentCandidate();
                Refresh();
                return true;
            } else if(event->key == InputKeyBack) {
                return false;
            }
        } else if(event->type == InputTypeLong) {
            if(isPaused && (event->key == InputKeyOk || event->key == InputKeyRight)) {
                isHolding = true;
                if(!isTransmitting) {
                    sendCurrentCandidate();
                }
                return true;
            }
        } else if(event->type == InputTypeRelease) {
            if(isHolding && (event->key == InputKeyOk || event->key == InputKeyRight)) {
                isHolding = false;
                subghz->PutToIdle();
                isTransmitting = false;
                Refresh();
                return true;
            }
        }
        return false;
    }

    void Refresh() {
        if(view != NULL) {
            view_commit_model(view, true);
        }
    }
};
