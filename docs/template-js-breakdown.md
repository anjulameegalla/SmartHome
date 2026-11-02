# index.html JavaScript Logic Breakdown

Here is the detailed breakdown of the JavaScript Logic, separated into functional blocks with code snippets and explanations.

## 1. Navigation Logic
This function handles the "Single Page Application" feel, allowing users to switch between sections (Dashboard, Controls, Analytics) without reloading the page.

```JavaScript
// Navigation Handling
function navigateTo(sectionId) {
    // 1. Scroll to the specific section
    document.getElementById(sectionId).scrollIntoView({behavior: 'smooth'});
    
    // 2. Reset active class (visual highlight) from all buttons
    document.querySelectorAll('.nav-item').forEach(el => el.classList.remove('active'));
    
    // 3. Set active class on the clicked button
    const navId = 'nav-' + sectionId;
    const activeNav = document.getElementById(navId);
    if(activeNav) activeNav.classList.add('active');
}
```

- `scrollIntoView({behavior: 'smooth'})`: This built-in browser method creates the smooth scrolling animation when you click a menu item.

- `classList.remove('active')`: It loops through every sidebar link and removes the blue highlight.

- `classList.add('active')`: It finds the specific link you just clicked (constructed by adding "`nav-`" to the ID) and adds the blue highlight back to it.


## 2. Live Data Updates
This is the heart of the dashboard. It fetches real-time data from the server and updates the HTML text and colors.

```JavaScript
async function updateData() {
    try {
        // 1. Fetch data from the server
        const response = await fetch('/data');
        const data = await response.json();
        
        // 2. Update Main Numbers
        document.getElementById('temp_live_p').innerText = data.temperature + "°";
        document.getElementById('humi_live_p').innerText = data.humidity + "%";
        
        // 3. Format Gas Sensor Data (avoid decimals)
        let gasVal = parseFloat(data.gas).toFixed(0);
        if (isNaN(gasVal)) gasVal = "--";
        
        document.getElementById('gas_live_p').innerText = gasVal + " PPM";
        document.getElementById('gas_card_p').innerText = gasVal + " PPM";

        // 4. Update Mini Cards
        document.getElementById('temp_card_p').innerText = data.temperature + " °C";
        document.getElementById('humi_card_p').innerText = data.humidity + " %";

        // 5. Motion Sensor Logic
        const motionText = document.getElementById('motion_live_p');
        const motionDot = document.getElementById('motion_dot');
        
        if (data.motion === 'DETECTED') {
            motionText.innerText = "Detected";
            motionText.style.color = "#ef4444"; // Red color
            motionDot.classList.remove('dot-green');
            motionDot.classList.add('dot-red');
            document.getElementById('motion_time').innerText = new Date().toLocaleTimeString();
        } else {
            motionText.innerText = "Clear";
            motionText.style.color = "#1f2937"; // Dark text color
            motionDot.classList.remove('dot-red');
            motionDot.classList.add('dot-green');
        }
    } catch (error) {
        console.error("Error fetching data:", error);
    }
}
```

- `async/await`: Used to handle the network request (fetch). It pauses the function until the server replies, preventing the UI from freezing.

- `parseFloat(...).toFixed(0)`: Used on the gas sensor to ensure you don't get messy numbers like 123.45678 PPM. It rounds to the nearest whole number.

- Motion Logic: This `if/else` block changes not just the text ("Detected" vs "Clear") but also the style. It swaps CSS classes (dot-red vs dot-green) to visually alert the user.


## 3. Hardware Controls
These functions send commands back to the server when you toggle a switch.

```JavaScript
// Example: Toggle LED
async function toggleLED() {
    // 1. Check if the checkbox is checked or unchecked
    const state = document.getElementById('ledSwitch').checked ? "ON" : "OFF";
    
    // 2. Send POST request to server
    await fetch('/control/led', { 
        method: 'POST', 
        headers: {'Content-Type': 'application/json'}, 
        body: JSON.stringify({ state: state }) 
    });
}
```

- Ternary Operator `(? :)`: checked ? "ON" : "OFF" is a shortcut. It means "If checked is true, use 'ON', otherwise use 'OFF'".

- `method: 'POST'`: Unlike updateData (which uses GET to retrieve), this uses POST to send data/commands.

- `JSON.stringify`: Converts the JavaScript object `{ state: "ON" }` into a text string that the server can read.

(Note: toggleRelay and toggleSensor logic is identical, just aiming at different URL endpoints.)


## 4. Chart Rendering
This is a reusable function designed to draw any of the three charts (Temp, Humidity, Gas) without rewriting code.

```JavaScript
async function createChart(canvasId, apiUrl, labelName, color) {
    try {
        // 1. Get History Data
        const response = await fetch(apiUrl);
        const apiData = await response.json();
        
        // 2. Setup Canvas Context
        const ctx = document.getElementById(canvasId).getContext('2d');
        
        // 3. Create Gradient Effect (fades out at bottom)
        let gradient = ctx.createLinearGradient(0, 0, 0, 400);
        gradient.addColorStop(0, color.replace('rgb', 'rgba').replace(')', ', 0.5)')); // 50% opacity
        gradient.addColorStop(1, color.replace('rgb', 'rgba').replace(')', ', 0.0)')); // 0% opacity

        // 4. Initialize Chart.js
        new Chart(ctx, {
            type: 'line',
            data: {
                labels: apiData.labels, // X-axis (Time)
                datasets: [{
                    label: labelName,
                    data: apiData.data, // Y-axis (Values)
                    borderColor: color,
                    backgroundColor: gradient,
                    borderWidth: 2,
                    fill: true,         // Fill area under line
                    tension: 0.4        // Smooth curves
                }]
            },
            options: { ... } // (Styling options hidden for brevity)
        });
    } catch (error) {
        console.error(`Error creating chart ${canvasId}:`, error);
    }
}
```

- Reusability: This function takes arguments (canvasId, apiUrl, color) so it can be called 3 different times for 3 different graphs.

- `gradient`: This manipulates the color string to create a "fade" effect under the line, making the chart look modern.

- `tension: 0.4`: This tells the library to smooth the line connecting the data points using Bezier curves.

## 5. Initialization
This code runs automatically when the page is fully loaded.

```JavaScript
window.onload = function() {
    // 1. Get data immediately (don't wait 3 seconds)
    updateData(); 
    
    // 2. Start the interval timer
    setInterval(updateData, 3000); 

    // 3. Draw the charts
    createChart('tempChart', '/api/history/temperature', 'Temperature', 'rgb(245, 158, 11)');
    createChart('humiChart', '/api/history/humidity', 'Humidity', 'rgb(59, 130, 246)');
    createChart('gasChart', '/api/history/gas', 'Air Quality', 'rgb(16, 185, 129)');
};
```

- `window.onload`: Ensures no code runs until the HTML elements actually exist.

- `setInterval(..., 3000)`: This creates the "live" effect. Every 3000ms (3 seconds), it runs `updateData()` again to refresh the sensor readings.

- Chart Colors: The RGB colors passed here match the colors used in the CSS for visual consistency (Amber for Temp, Blue for Humidity, Green for Gas).


<br>