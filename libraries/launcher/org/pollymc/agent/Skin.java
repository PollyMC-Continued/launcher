// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (C) 2026 PollyMC Continued Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
package org.pollymc.agent;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.net.HttpURLConnection;
import java.net.Proxy;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandler;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.Base64;
import java.util.Hashtable;

/**
 * Serves locally stored player skins for offline accounts without claiming the
 * JVM-wide {@link URLStreamHandlerFactory} slot.
 *
 * <p>Some loaders (NeoForge via securejarhandler's {@code ModuleClassLoader},
 * Quilt via its delegating URL factory) need {@code URL.setURLStreamHandlerFactory}
 * for themselves, and that method can only be called once per JVM. Instead of
 * taking the factory slot, this helper registers a per-protocol handler through
 * the {@code java.protocol.handler.pkgs} system property. {@code URL} consults
 * that property after the factory, and the loaders' factories return {@code null}
 * for {@code https}, so our handler still intercepts profile requests but does not
 * conflict with the loaders.
 */
public final class Skin {

    private static final String HANDLER_PKG = "org.pollymc.agent.protocol";
    static final String SESSION_HOST = "sessionserver.mojang.com";
    static final String PROFILE_PATH = "/session/minecraft/profile/";
    static final String TEXTURE_HOST = "textures.minecraft.net";
    // Texture URLs must be served under /texture/: authlib 10 (shipped with Minecraft 25.x+)
    // validates every texture URL in a profile payload against the "validUris" whitelist of
    // the discovery document (https://discovery.minecraftservices.com/minecraft/client), which
    // only allows https://textures.minecraft.net/texture/{textureId}. Older authlib releases
    // only checked the domain, so our old /skin/ path stopped working and offline skins
    // silently fell back to the default skin. /skin/ is still *served* so game versions that
    // cached the old URLs (and old authlib) keep working.
    static final String TEXTURE_PATH_PREFIX = "/texture/";
    static final String LEGACY_SKIN_PATH_PREFIX = "/skin/";

    private static volatile URLStreamHandler originalHttpsHandler;

    private Skin() {}

    public static void install() {
        originalHttpsHandler = captureOriginalHandler("https");
        clearCachedHandler("https");

        String existing = System.getProperty("java.protocol.handler.pkgs");
        if (existing == null || existing.isEmpty()) {
            System.setProperty("java.protocol.handler.pkgs", HANDLER_PKG);
        } else if (!existing.contains(HANDLER_PKG)) {
            System.setProperty("java.protocol.handler.pkgs", existing + "|" + HANDLER_PKG);
        }
    }

    private static URLStreamHandler captureOriginalHandler(String protocol) {
        try {
            Field handlersField = URL.class.getDeclaredField("handlers");
            handlersField.setAccessible(true);
            @SuppressWarnings("unchecked")
            Hashtable<String, URLStreamHandler> handlers =
                (Hashtable<String, URLStreamHandler>) handlersField.get(null);
            URLStreamHandler handler = handlers.get(protocol);
            if (handler != null) {
                return handler;
            }
        } catch (Exception e) {
            System.err.println("[PollyMC-Continued] Could not capture handler from table: " + e.getMessage());
        }
        try {
            URL dummy = new URL("https://localhost");
            Field handlerField = URL.class.getDeclaredField("handler");
            handlerField.setAccessible(true);
            return (URLStreamHandler) handlerField.get(dummy);
        } catch (Exception e) {
            System.err.println("[PollyMC-Continued] Could not capture default handler: " + e.getMessage());
        }
        return null;
    }

    private static void clearCachedHandler(String protocol) {
        try {
            Field handlersField = URL.class.getDeclaredField("handlers");
            handlersField.setAccessible(true);
            @SuppressWarnings("unchecked")
            Hashtable<String, URLStreamHandler> handlers =
                (Hashtable<String, URLStreamHandler>) handlersField.get(null);
            handlers.remove(protocol);
        } catch (Exception e) {
            System.err.println("[PollyMC-Continued] Could not clear cached handler: " + e.getMessage());
        }
    }

    public static URLConnection open(URL url, Proxy proxy) throws IOException {
        String host = url.getHost();
        if (SESSION_HOST.equals(host)) {
            String path = url.getPath();
            if (path != null && path.startsWith(PROFILE_PATH)) {
                String uuid = path.substring(PROFILE_PATH.length()).replace("-", "");
                String skinDir = findSkinDir(uuid);
                if (skinDir != null) {
                    File skinFile = new File(skinDir, "skin.png");
                    if (skinFile.exists()) {
                        return new LocalSkinURLConnection(url, skinFile, uuid);
                    }
                }
            }
        } else if (TEXTURE_HOST.equals(host)) {
            String path = url.getPath();
            String prefix = texturePathPrefix(path);
            if (prefix != null) {
                // Texture file name is "<uuid>_<sha1 of skin.png>.png"; the hash only exists to bust
                // Minecraft's on-disk skin cache, which is keyed by the URL's base name.
                String name = path.substring(prefix.length()).replace(".png", "");
                int sep = name.indexOf('_');
                String uuid = (sep >= 0 ? name.substring(0, sep) : name).replace("-", "");
                String skinDir = findSkinDir(uuid);
                if (skinDir != null) {
                    File skinFile = new File(skinDir, "skin.png");
                    if (skinFile.exists()) {
                        return new LocalSkinTextureURLConnection(url, skinFile);
                    }
                }
            }
        }

        URLStreamHandler fallback = originalHttpsHandler;
        if (fallback != null) {
            try {
                Method m = URLStreamHandler.class.getDeclaredMethod("openConnection", URL.class, Proxy.class);
                m.setAccessible(true);
                return (URLConnection) m.invoke(fallback, url, proxy);
            } catch (Exception e) {
                try {
                    Method m = URLStreamHandler.class.getDeclaredMethod("openConnection", URL.class);
                    m.setAccessible(true);
                    return (URLConnection) m.invoke(fallback, url);
                } catch (Exception e2) {
                    throw new IOException("Cannot connect to " + url, e2);
                }
            }
        }
        // No original handler available: connect directly through the internal implementation
        try {
            Class<?> implClass = Class.forName("sun.net.www.protocol.https.HttpsURLConnectionImpl");
            Constructor<?> ctor = implClass.getDeclaredConstructor(URL.class, Proxy.class);
            ctor.setAccessible(true);
            return (HttpURLConnection) ctor.newInstance(url, proxy != null ? proxy : Proxy.NO_PROXY);
        } catch (Exception e) {
            throw new IOException("Cannot create HTTPS connection to " + url + " - " + e.getMessage());
        }
    }

    private static String texturePathPrefix(String path) {
        if (path == null) {
            return null;
        }
        if (path.startsWith(TEXTURE_PATH_PREFIX)) {
            return TEXTURE_PATH_PREFIX;
        }
        if (path.startsWith(LEGACY_SKIN_PATH_PREFIX)) {
            return LEGACY_SKIN_PATH_PREFIX;
        }
        return null;
    }

    /**
     * Hex SHA-1 of the skin file. Used as part of the texture URL so that a changed skin
     * gets a new URL (and therefore a new Minecraft skin-cache key).
     */
    static String skinHash(File skinFile) {
        try {
            byte[] digest = MessageDigest.getInstance("SHA-1").digest(Files.readAllBytes(skinFile.toPath()));
            StringBuilder sb = new StringBuilder(digest.length * 2);
            for (byte b : digest) {
                sb.append(String.format("%02x", b));
            }
            return sb.toString();
        } catch (Exception e) {
            // Fall back to something that still changes whenever the file is rewritten.
            return Long.toHexString(skinFile.lastModified()) + Long.toHexString(skinFile.length());
        }
    }

    private static String findSkinDir(String uuid) {
        for (String basePath : searchPaths()) {
            File dir = new File(basePath, "skins/" + uuid);
            if (dir.isDirectory()) {
                return dir.getAbsolutePath();
            }
        }
        return null;
    }

    static String findAccountName(String uuid) {
        for (String basePath : searchPaths()) {
            File accountsFile = new File(basePath, "accounts.json");
            if (!accountsFile.isFile()) {
                continue;
            }
            try {
                String content = new String(Files.readAllBytes(accountsFile.toPath()), StandardCharsets.UTF_8);
                int idx = content.indexOf("\"profile\"");
                while (idx >= 0) {
                    int objStart = content.indexOf('{', idx);
                    int objEnd = content.indexOf('}', objStart);
                    if (objStart < 0 || objEnd < 0) {
                        break;
                    }
                    String block = content.substring(objStart, objEnd + 1);
                    String id = extractJsonString(block, "\"id\"");
                    if (id != null && id.replace("-", "").equalsIgnoreCase(uuid)) {
                        String name = extractJsonString(block, "\"name\"");
                        if (name != null && !name.isEmpty()) {
                            return name;
                        }
                    }
                    idx = content.indexOf("\"profile\"", objEnd);
                }
            } catch (Exception ignored) {}
        }
        return null;
    }

    private static String extractJsonString(String json, String key) {
        int keyIdx = json.indexOf(key);
        if (keyIdx < 0) {
            return null;
        }
        int colonIdx = json.indexOf(':', keyIdx);
        if (colonIdx < 0) {
            return null;
        }
        int quoteIdx = json.indexOf('"', colonIdx);
        if (quoteIdx < 0) {
            return null;
        }
        int endQuoteIdx = json.indexOf('"', quoteIdx + 1);
        if (endQuoteIdx < 0) {
            return null;
        }
        return json.substring(quoteIdx + 1, endQuoteIdx);
    }

    private static String[] searchPaths() {
        String[] paths = {
            System.getProperty("pollymc.datadir", ""),
            System.getProperty("user.home") + "/.local/share/PollyMC",
            System.getProperty("user.home") + "/AppData/Roaming/PollyMC",
            System.getProperty("user.home") + "/Library/Application Support/PollyMC"
        };
        return paths;
    }
}

class LocalSkinURLConnection extends HttpURLConnection {

    private final File skinFile;
    private final String uuid;

    LocalSkinURLConnection(URL url, File skinFile, String uuid) {
        super(url);
        this.skinFile = skinFile;
        this.uuid = uuid;
    }

    @Override
    public void connect() {
        connected = true;
    }

    @Override
    public InputStream getInputStream() throws IOException {
        if (!connected) {
            connect();
        }

        String model = "classic";
        File metaFile = new File(skinFile.getParentFile(), "skin.json");
        if (metaFile.exists()) {
            try {
                BufferedReader reader = new BufferedReader(new FileReader(metaFile));
                String line;
                while ((line = reader.readLine()) != null) {
                    if (line.contains("\"model\"") && line.toUpperCase().contains("SLIM")) {
                        model = "slim";
                    }
                }
                reader.close();
            } catch (Exception ignored) {}
        }

        // The hash makes the URL (and Minecraft's skin cache key) change whenever the skin changes.
        // The /texture/ path is required by the authlib 10 discovery "validUris" whitelist; see
        // the comment on TEXTURE_PATH_PREFIX above.
        String skinUrl = "https://" + Skin.TEXTURE_HOST + Skin.TEXTURE_PATH_PREFIX + uuid + "_" + Skin.skinHash(skinFile) + ".png";

        String profileName = Skin.findAccountName(uuid);
        if (profileName == null) {
            profileName = "Player";
        }

        String texturesJson = "{\"timestamp\":" + System.currentTimeMillis()
            + ",\"profileId\":\"" + uuid + "\""
            + ",\"profileName\":\"" + profileName + "\""
            + ",\"textures\":{\"SKIN\":{\"url\":\"" + skinUrl + "\""
            + (model.equals("slim") ? ",\"metadata\":{\"model\":\"slim\"}" : "")
            + "}}}";

        String base64 = Base64.getEncoder().encodeToString(texturesJson.getBytes(StandardCharsets.UTF_8));

        String responseJson = "{\"id\":\"" + uuid + "\""
            + ",\"name\":\"" + profileName + "\""
            + ",\"properties\":[{\"name\":\"textures\",\"value\":\"" + base64 + "\"}]}";

        responseCode = 200;
        return new ByteArrayInputStream(responseJson.getBytes(StandardCharsets.UTF_8));
    }

    @Override
    public int getResponseCode() {
        return 200;
    }

    @Override
    public String getContentType() {
        return "application/json";
    }

    @Override
    public long getContentLengthLong() {
        return -1;
    }

    @Override
    public boolean usingProxy() {
        return false;
    }

    @Override
    public void disconnect() {
        connected = false;
    }
}

class LocalSkinTextureURLConnection extends HttpURLConnection {

    private final File skinFile;

    LocalSkinTextureURLConnection(URL url, File skinFile) {
        super(url);
        this.skinFile = skinFile;
    }

    @Override
    public void connect() {
        connected = true;
    }

    @Override
    public InputStream getInputStream() throws IOException {
        if (!connected) {
            connect();
        }
        byte[] data = Files.readAllBytes(skinFile.toPath());
        responseCode = 200;
        return new ByteArrayInputStream(data);
    }

    @Override
    public int getResponseCode() {
        return 200;
    }

    @Override
    public String getContentType() {
        return "image/png";
    }

    @Override
    public long getContentLengthLong() {
        try {
            return Files.size(skinFile.toPath());
        } catch (IOException e) {
            return -1;
        }
    }

    @Override
    public boolean usingProxy() {
        return false;
    }

    @Override
    public void disconnect() {
        connected = false;
    }
}
