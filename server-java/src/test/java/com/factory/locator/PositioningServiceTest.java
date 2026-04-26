package com.factory.locator;

import com.factory.locator.dto.LocationResponse;
import com.factory.locator.dto.ScanPayload;
import com.factory.locator.service.PositioningService;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.core.io.ClassPathResource;

import java.io.InputStream;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest
class PositioningServiceTest {

    @Autowired
    private PositioningService positioningService;

    @Autowired
    private ObjectMapper objectMapper;

    @Test
    void shouldLocateDeviceFromMockScan() throws Exception {
        try (InputStream inputStream = new ClassPathResource("mock-data/scan_example.json").getInputStream()) {
            ScanPayload payload = objectMapper.readValue(inputStream, ScanPayload.class);
            LocationResponse response = positioningService.locate(payload);

            assertThat(response.deviceId()).isEqualTo("rack-tag-001");
            assertThat(response.apCountUsed()).isEqualTo(5);
            assertThat(response.qualityScore()).isGreaterThan(0.4);
            assertThat(Math.abs(response.position().xM())).isLessThan(20.0);
            assertThat(Math.abs(response.position().yM())).isLessThan(20.0);
        }
    }

    @Test
    void shouldConvertStrongerRssiToShorterDistance() {
        double near = positioningService.rssiToDistance(-50, -42, 2.1);
        double far = positioningService.rssiToDistance(-70, -42, 2.1);
        assertThat(near).isLessThan(far);
    }
}
