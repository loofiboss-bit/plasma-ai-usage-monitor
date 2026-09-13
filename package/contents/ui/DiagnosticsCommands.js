function versionCheckCommand(productType) {
    if (productType === "fedora")
        return "plasmashell --version; rpm -q plasma-ai-usage-monitor";
    return "plasmashell --version";
}
