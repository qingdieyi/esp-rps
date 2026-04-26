package com.factory.locator.dto;

import com.fasterxml.jackson.annotation.JsonProperty;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;

public record ScanResult(
        @NotBlank
        String bssid,

        @NotBlank
        String ssid,

        int rssi,

        @JsonProperty("primary_channel")
        int primaryChannel,

        @JsonProperty("secondary_channel")
        int secondaryChannel,

        @JsonProperty("is_backhaul")
        @NotNull
        Boolean isBackhaul
) {
}
