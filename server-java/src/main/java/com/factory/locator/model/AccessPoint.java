package com.factory.locator.model;

import com.fasterxml.jackson.annotation.JsonProperty;

public record AccessPoint(
        String bssid,
        double lat,
        double lon,
        int floor,
        @JsonProperty("tx_power_dbm_at_1m")
        double txPowerDbmAt1m,
        @JsonProperty("path_loss_exponent")
        double pathLossExponent
) {
}
