package com.factory.locator.controller;

import com.factory.locator.dto.LocationResponse;
import com.factory.locator.dto.ScanPayload;
import com.factory.locator.service.PositioningService;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/api/v1")
public class LocationController {

    private static final Logger log = LoggerFactory.getLogger(LocationController.class);
    private final PositioningService positioningService;

    public LocationController(PositioningService positioningService) {
        this.positioningService = positioningService;
    }

    @GetMapping("/health")
    public String health() {
        return "ok";
    }

    @PostMapping("/locate")
    public LocationResponse locate(@RequestBody ScanPayload payload) {
        int scanCount = payload.scanResults() == null ? 0 : payload.scanResults().size();
        log.info("Received locate request, deviceId={}, deviceMac={}, scanCount={}",
                payload.deviceId(),
                payload.deviceMac(),
                scanCount);
        if (scanCount > 0) {
            log.info("First AP sample, bssid={}, ssid={}, rssi={}",
                    payload.scanResults().get(0).bssid(),
                    payload.scanResults().get(0).ssid(),
                    payload.scanResults().get(0).rssi());
        }
        return positioningService.locate(payload);
    }
}
