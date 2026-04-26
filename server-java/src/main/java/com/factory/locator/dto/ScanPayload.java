package com.factory.locator.dto;

import com.fasterxml.jackson.annotation.JsonProperty;
import jakarta.validation.Valid;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotEmpty;
import jakarta.validation.constraints.NotNull;

import java.util.List;

public record ScanPayload(
        @JsonProperty("device_id")
        @NotBlank
        String deviceId,

        @JsonProperty("device_mac")
        @NotBlank
        String deviceMac,

        @JsonProperty("timestamp_ms")
        @NotNull
        Long timestampMs,

        @JsonProperty("scan_results")
        @NotEmpty
        List<@Valid ScanResult> scanResults
) {
}
