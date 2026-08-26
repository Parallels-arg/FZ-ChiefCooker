#pragma once

#include "lib/HandlerContext.hpp"
#include "lib/String.hpp"
#include "app/pager/PagerReceiver.hpp"
#include "lib/hardware/subghz/SubGhzModule.hpp"
#include "lib/ui/view/UiView.hpp"
#include "lib/ui/view/VariableItemListUiView.hpp"
#include "lib/ui/view/item/UiVariableItem.hpp"
#include "lib/FlipperDolphin.hpp"
#include "lib/ui/UiManager.hpp"
#include "app/AppConfig.hpp"
#include "BruteForceRunScreen.hpp"

class BruteForceSettingsScreen {
private:
    AppConfig* config;
    SubGhzModule* subghz;
    PagerReceiver* receiver;

    VariableItemListUiView* varItemList;

    UiVariableItem* speedItem = NULL;
    UiVariableItem* encodingItem = NULL;
    UiVariableItem* stationMinItem = NULL;
    UiVariableItem* stationMaxItem = NULL;
    UiVariableItem* pagerMinItem = NULL;
    UiVariableItem* pagerMaxItem = NULL;
    UiVariableItem* startItem = NULL;

    String speedStr;
    String encodingStr;
    String stationMinStr;
    String stationMaxStr;
    String pagerMinStr;
    String pagerMaxStr;

    int32_t startItemIndex = -1;

    uint8_t speedIndex = 0; // 0=Max, 1=Normal, 2=Slow
    uint8_t encodingIndex = 0; // 0=ALL, 1..5=Decoders 0..4
    uint16_t stationMin = 0;
    uint16_t stationMax = 8191;
    uint16_t pagerMin = 1;
    uint16_t pagerMax = 10;

    static const uint16_t STATION_STEP = 50;

public:
    BruteForceSettingsScreen(
        AppConfig* config,
        SubGhzModule* subghz,
        PagerReceiver* receiver
    ) {
        this->config = config;
        this->subghz = subghz;
        this->receiver = receiver;

        varItemList = new VariableItemListUiView();
        varItemList->SetOnDestroyHandler(HANDLER(&BruteForceSettingsScreen::destroy));
        varItemList->SetEnterPressHandler(HANDLER_1ARG(&BruteForceSettingsScreen::enterPressed));

        varItemList->AddItem(
            speedItem = new UiVariableItem(
                "Speed", speedIndex, 3, HANDLER_1ARG(&BruteForceSettingsScreen::speedValueChanged)
            )
        );

        // Encoding: index 0 is ALL, indices 1..N are specific decoders
        varItemList->AddItem(
            encodingItem = new UiVariableItem(
                "Encoding", encodingIndex, receiver->decodersCount + 1, HANDLER_1ARG(&BruteForceSettingsScreen::encodingValueChanged)
            )
        );

        varItemList->AddItem(
            stationMinItem = new UiVariableItem("Station Min", 0, 1, HANDLER_1ARG(&BruteForceSettingsScreen::stationMinValueChanged))
        );
        varItemList->AddItem(
            stationMaxItem = new UiVariableItem("Station Max", 0, 1, HANDLER_1ARG(&BruteForceSettingsScreen::stationMaxValueChanged))
        );

        varItemList->AddItem(
            pagerMinItem = new UiVariableItem("Pager Min", pagerMin - 1, 99, HANDLER_1ARG(&BruteForceSettingsScreen::pagerMinValueChanged))
        );
        varItemList->AddItem(
            pagerMaxItem = new UiVariableItem("Pager Max", pagerMax - 1, 99, HANDLER_1ARG(&BruteForceSettingsScreen::pagerMaxValueChanged))
        );

        startItemIndex = varItemList->AddItem(startItem = new UiVariableItem("Start Brute Force", ""));

        updateStationBounds();
    }

private:
    bool isTryAll() {
        return encodingIndex == 0;
    }

    uint8_t getSelectedDecoderIndex() {
        return encodingIndex > 0 ? encodingIndex - 1 : 0;
    }

    uint16_t getMaxStationForCurrentEncoding() {
        if(isTryAll()) {
            return 8191;
        }
        return receiver->decoders[getSelectedDecoderIndex()]->GetMaxStation();
    }

    void updateStationBounds() {
        if(stationMinItem == NULL || stationMaxItem == NULL) {
            return;
        }

        uint16_t maxAllowed = getMaxStationForCurrentEncoding();

        if(isTryAll()) {
            stationMin = 0;
            stationMax = maxAllowed;
            stationMinItem->SetSelectedItem(0, 1);
            stationMaxItem->SetSelectedItem(0, 1);
        } else {
            if(stationMax > maxAllowed) stationMax = maxAllowed;
            if(stationMin > maxAllowed) stationMin = 0;

            uint8_t minStepsCount = (maxAllowed / STATION_STEP) + 1;
            uint8_t minSelIdx = stationMin / STATION_STEP;
            if(minSelIdx >= minStepsCount) minSelIdx = minStepsCount - 1;
            stationMinItem->SetSelectedItem(minSelIdx, minStepsCount);

            uint8_t maxStepsCount = (maxAllowed / STATION_STEP) + 1;
            uint8_t maxSelIdx = stationMax / STATION_STEP;
            if(maxSelIdx >= maxStepsCount) maxSelIdx = maxStepsCount - 1;
            stationMaxItem->SetSelectedItem(maxSelIdx, maxStepsCount);
        }
    }

    void enterPressed(int32_t index) {
        if(index == startItemIndex) {
            startBruteForce();
        }
    }

    void startBruteForce() {
        BruteForceRunScreen* runScreen = new BruteForceRunScreen(
            config,
            subghz,
            receiver,
            speedIndex,
            isTryAll(),
            getSelectedDecoderIndex(),
            stationMin,
            stationMax,
            pagerMin,
            pagerMax
        );
        UiManager::GetInstance()->PushView(runScreen);
    }

    const char* speedValueChanged(uint8_t index) {
        speedIndex = index;
        switch(index) {
        case 0:
            return "Max (Fastest)";
        case 1:
            return "Normal";
        case 2:
            return "Slow (Reliable)";
        default:
            return "Max";
        }
    }

    const char* encodingValueChanged(uint8_t index) {
        encodingIndex = index;
        updateStationBounds();

        if(index == 0) {
            return "ALL (Try All)";
        } else {
            return receiver->decoders[index - 1]->GetShortName();
        }
    }

    const char* stationMinValueChanged(uint8_t stepIndex) {
        if(isTryAll()) {
            return "Auto (0)";
        }
        stationMin = stepIndex * STATION_STEP;
        if(stationMin > stationMax) {
            stationMin = stationMax;
        }
        return stationMinStr.fromInt(stationMin);
    }

    const char* stationMaxValueChanged(uint8_t stepIndex) {
        if(isTryAll()) {
            return "Auto (Max)";
        }
        uint16_t maxAllowed = getMaxStationForCurrentEncoding();
        stationMax = stepIndex * STATION_STEP;
        if(stationMax > maxAllowed) {
            stationMax = maxAllowed;
        }
        if(stationMax < stationMin) {
            stationMax = stationMin;
        }
        return stationMaxStr.fromInt(stationMax);
    }

    const char* pagerMinValueChanged(uint8_t index) {
        pagerMin = index + 1;
        if(pagerMin > pagerMax) {
            pagerMax = pagerMin;
            if(pagerMaxItem != NULL) {
                pagerMaxItem->SetSelectedItem(pagerMax - 1, 99);
            }
        }
        return pagerMinStr.fromInt(pagerMin);
    }

    const char* pagerMaxValueChanged(uint8_t index) {
        pagerMax = index + 1;
        if(pagerMax < pagerMin) {
            pagerMin = pagerMax;
            if(pagerMinItem != NULL) {
                pagerMinItem->SetSelectedItem(pagerMin - 1, 99);
            }
        }
        return pagerMaxStr.fromInt(pagerMax);
    }

    void destroy() {
        delete speedItem;
        delete encodingItem;
        delete stationMinItem;
        delete stationMaxItem;
        delete pagerMinItem;
        delete pagerMaxItem;
        delete startItem;
        delete subghz;
        delete receiver;
        delete this;
    }

public:
    UiView* GetView() {
        return varItemList;
    }
};
