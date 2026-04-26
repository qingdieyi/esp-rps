package com.factory.locator.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

public record AccessPointDistance(
        String bssid,
        int rssi,
        @JsonProperty("estimated_distance_m")
        double estimatedDistanceM
) {
}
