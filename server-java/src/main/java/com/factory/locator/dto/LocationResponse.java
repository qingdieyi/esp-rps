package com.factory.locator.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

import java.util.List;

public record LocationResponse(
        @JsonProperty("device_id")
        String deviceId,
        PositionResult position,
        @JsonProperty("quality_score")
        double qualityScore,
        @JsonProperty("ap_count_used")
        int apCountUsed,
        List<AccessPointDistance> distances
) {
}
