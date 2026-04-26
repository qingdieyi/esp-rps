package com.factory.locator.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

public record ErrorResponse(
        String error,
        String message,
        @JsonProperty("device_id")
        String deviceId
) {
}
