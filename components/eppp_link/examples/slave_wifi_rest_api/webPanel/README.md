# ESP32 Wi-Fi Manager Web Interface

A simple HTML+JS page to manage ESP32 Wi-Fi connections via REST API.
Allows you to **scan networks**, **connect**, **disconnect**, and **monitor Wi-Fi status** directly from your browser.

## Features

* **View Wi-Fi status:** connected/disconnected, SSID, RSSI
* **Scan for available Wi-Fi networks**
* **Connect** to a Wi-Fi network by SSID and password
* **Disconnect** from Wi-Fi
* **Auto-fill SSID** from scanned networks list
* **Mobile-friendly**, lightweight, no build tools needed
* Works out of the box with the [modified ESP32 REST API firmware](../)

## How to Use

### 1. Configure ESP32 IP Address

**Important:**
You must set the correct ESP32 IP address (from the PPP network, e.g., `192.168.11.2`) in the script before using the page.

Find and update this line in your HTML:

```js
const API_BASE = "http://<YOUR_ESP32_IP_IN_PPP_NETWORK>";
```

Replace `<YOUR_ESP32_IP_IN_PPP_NETWORK>` with your ESP32's actual IP address.

**Example:**

```js
const API_BASE = "http://192.168.11.2";
```

If you are unsure of the IP address, check your PPP server log, your router's DHCP leases, or the serial output of the ESP32.

### 2. Open the Page

* Open `index.html` in any modern browser (desktop or mobile).
* Make sure your computer or phone is in the same network as your ESP32.

### 3. Usage

* **Status:** See if ESP32 is connected and to which network.
* **Scan Networks:** Click "Scan Networks" to see available Wi-Fi networks. Click "Select" to autofill SSID in the form.
* **Connect to Wi-Fi:** Enter SSID and password (or use "Select"), then click "Connect".
* **Disconnect:** Click "Disconnect" to disconnect from Wi-Fi.
* Status refreshes automatically after actions.

## Troubleshooting

* **"ESP32 request error"**

  * Check that `API_BASE` is correct.
  * Ensure ESP32 is running and accessible on the network.
  * Make sure the REST API firmware is flashed and running.
  * Check browser/network firewalls and NAT settings.

* **No networks found**

  * Move ESP32 closer to Wi-Fi routers/APs.
  * Check ESP32 logs for scan errors.

* **CORS errors in browser**

  * The firmware should include `Access-Control-Allow-Origin: *` headers by default.

## Customization

* Style the page using the CSS in `<style>`.
* You can embed this UI into any admin panel, dashboard, or mobile wrapper.
* To add new REST endpoints (for device info, reboot, etc.), extend both the firmware and this page’s logic.

## Credits

* Frontend by \glarionenko, \2025
* Backend REST API — see [eppp\_link/slave\_wifi\_rest\_api/](../)
