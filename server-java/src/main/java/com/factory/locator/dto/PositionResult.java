package com.factory.locator.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

public record PositionResult(
        double lat,
        double lon,
        int floor,
        @JsonProperty("x_m")
        double xM,
        @JsonProperty("y_m")
        double yM
) {
}
