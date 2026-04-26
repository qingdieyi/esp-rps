package com.factory.locator.service;

import com.factory.locator.dto.AccessPointDistance;
import com.factory.locator.dto.LocationResponse;
import com.factory.locator.dto.PositionResult;
import com.factory.locator.dto.ScanPayload;
import com.factory.locator.dto.ScanResult;
import com.factory.locator.model.AccessPoint;
import com.factory.locator.repository.AccessPointRepository;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

import java.util.Comparator;
import java.util.List;
import java.util.Map;

@Service
public class PositioningService {

    private static final Logger log = LoggerFactory.getLogger(PositioningService.class);
    private final AccessPointRepository accessPointRepository;

    public PositioningService(AccessPointRepository accessPointRepository) {
        this.accessPointRepository = accessPointRepository;
    }

    public LocationResponse locate(ScanPayload payload) {
        Map<String, AccessPoint> accessPointMap = accessPointRepository.findAllAsMap();

        List<ScanResult> knownSamples = payload.scanResults().stream()
                .filter(sample -> accessPointMap.containsKey(sample.bssid()))
                .sorted(Comparator.comparingInt(ScanResult::rssi).reversed())
                .limit(6)
                .toList();

        log.info("Locate request matched {} known APs out of {} scan results",
                knownSamples.size(),
                payload.scanResults().size());

        if (knownSamples.size() < 3) {
            String knownBssids = knownSamples.stream()
                    .map(ScanResult::bssid)
                    .reduce((left, right) -> left + "," + right)
                    .orElse("");
            throw new IllegalArgumentException(
                    "Need at least three AP scan results with known coordinates. matched_bssids=" + knownBssids
            );
        }

        AccessPoint origin = accessPointMap.get(knownSamples.get(0).bssid());
        Point estimate = trilaterationRefine(knownSamples, accessPointMap);
        LatLon latLon = localXyToLatLon(estimate.x(), estimate.y(), origin.lat(), origin.lon());

        double quality = 0.0;
        List<AccessPointDistance> distances = knownSamples.stream()
                .map(sample -> {
                    double distance = rssiToDistance(
                            sample.rssi(),
                            accessPointMap.get(sample.bssid()).txPowerDbmAt1m(),
                            accessPointMap.get(sample.bssid()).pathLossExponent()
                    );
                    return new AccessPointDistance(sample.bssid(), sample.rssi(), round(distance, 2));
                })
                .toList();

        for (ScanResult sample : knownSamples) {
            quality += Math.min(Math.max((sample.rssi() + 90.0) / 45.0, 0.0), 1.0);
        }

        return new LocationResponse(
                payload.deviceId(),
                new PositionResult(
                        round(latLon.lat(), 7),
                        round(latLon.lon(), 7),
                        origin.floor(),
                        round(estimate.x(), 2),
                        round(estimate.y(), 2)
                ),
                round(quality / knownSamples.size(), 3),
                knownSamples.size(),
                distances
        );
    }

    public double rssiToDistance(double rssi, double txPowerDbmAt1m, double pathLossExponent) {
        return Math.pow(10.0, (txPowerDbmAt1m - rssi) / (10.0 * pathLossExponent));
    }

    Point weightedCentroid(List<ScanResult> samples, Map<String, AccessPoint> apTable) {
        if (samples.size() < 3) {
            throw new IllegalArgumentException("At least three known APs are required for positioning");
        }

        AccessPoint origin = apTable.get(samples.get(0).bssid());
        double numeratorX = 0.0;
        double numeratorY = 0.0;
        double denominator = 0.0;

        for (ScanResult sample : samples) {
            AccessPoint accessPoint = apTable.get(sample.bssid());
            Point apPoint = latLonToLocalXy(accessPoint.lat(), accessPoint.lon(), origin.lat(), origin.lon());
            double distance = rssiToDistance(sample.rssi(), accessPoint.txPowerDbmAt1m(), accessPoint.pathLossExponent());
            double weight = 1.0 / Math.max(distance * distance, 0.25);
            numeratorX += apPoint.x() * weight;
            numeratorY += apPoint.y() * weight;
            denominator += weight;
        }

        return new Point(numeratorX / denominator, numeratorY / denominator);
    }

    Point trilaterationRefine(List<ScanResult> samples, Map<String, AccessPoint> apTable) {
        AccessPoint origin = apTable.get(samples.get(0).bssid());
        Point point = weightedCentroid(samples, apTable);
        double x = point.x();
        double y = point.y();

        for (int i = 0; i < 12; i++) {
            double gradX = 0.0;
            double gradY = 0.0;

            for (ScanResult sample : samples) {
                AccessPoint accessPoint = apTable.get(sample.bssid());
                Point apPoint = latLonToLocalXy(accessPoint.lat(), accessPoint.lon(), origin.lat(), origin.lon());
                double expectedDistance = rssiToDistance(sample.rssi(), accessPoint.txPowerDbmAt1m(), accessPoint.pathLossExponent());
                double dx = x - apPoint.x();
                double dy = y - apPoint.y();
                double actualDistance = Math.max(Math.sqrt(dx * dx + dy * dy), 0.1);
                double residual = actualDistance - expectedDistance;
                double robustWeight = 1.0 / Math.max(Math.abs(residual), 1.0);

                gradX += robustWeight * residual * dx / actualDistance;
                gradY += robustWeight * residual * dy / actualDistance;
            }

            x -= 0.35 * gradX;
            y -= 0.35 * gradY;
        }

        return new Point(x, y);
    }

    Point latLonToLocalXy(double lat, double lon, double originLat, double originLon) {
        double metersPerDegLat = 111320.0;
        double metersPerDegLon = 111320.0 * Math.cos(Math.toRadians(originLat));
        double x = (lon - originLon) * metersPerDegLon;
        double y = (lat - originLat) * metersPerDegLat;
        return new Point(x, y);
    }

    LatLon localXyToLatLon(double x, double y, double originLat, double originLon) {
        double metersPerDegLat = 111320.0;
        double metersPerDegLon = 111320.0 * Math.cos(Math.toRadians(originLat));
        double lat = originLat + (y / metersPerDegLat);
        double lon = originLon + (x / metersPerDegLon);
        return new LatLon(lat, lon);
    }

    private double round(double value, int digits) {
        double factor = Math.pow(10, digits);
        return Math.round(value * factor) / factor;
    }

    record Point(double x, double y) {
    }

    record LatLon(double lat, double lon) {
    }
}
