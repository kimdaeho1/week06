function FindProxyForURL(url, host) {
    if (shExpMatch(url, "http://3.36.158.33:9000/*")) {
        return "PROXY 3.36.158.33:8000";
    }
    return "DIRECT";
}