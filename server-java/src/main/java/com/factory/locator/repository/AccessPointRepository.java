package com.factory.locator.repository;

import com.factory.locator.model.AccessPoint;
import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;
import jakarta.annotation.PostConstruct;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.core.io.Resource;
import org.springframework.stereotype.Repository;

import java.io.IOException;
import java.io.InputStream;
import java.io.UncheckedIOException;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.stream.Collectors;

@Repository
public class AccessPointRepository {

    private final ObjectMapper objectMapper;
    private final Resource registryResource;
    private Map<String, AccessPoint> accessPointMap = new ConcurrentHashMap<>();

    public AccessPointRepository(
            ObjectMapper objectMapper,
            @Value("${locator.ap-registry}") Resource registryResource
    ) {
        this.objectMapper = objectMapper;
        this.registryResource = registryResource;
    }

    @PostConstruct
    public void load() {
        try (InputStream inputStream = registryResource.getInputStream()) {
            List<AccessPoint> accessPoints = objectMapper.readValue(
                    inputStream,
                    new TypeReference<>() {
                    }
            );
            accessPointMap = accessPoints.stream()
                    .collect(Collectors.toConcurrentMap(AccessPoint::bssid, ap -> ap));
        } catch (IOException ex) {
            throw new UncheckedIOException("Failed to load AP registry", ex);
        }
    }

    public Map<String, AccessPoint> findAllAsMap() {
        return accessPointMap;
    }
}
