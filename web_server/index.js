// document element for displaying API results of switch section
const switchMessageDisp = document.getElementById("switch-message-display")

// document element for displaying API results of zcs section
const zcsMessageDisp = document.getElementById("zcs-message-display")

// document element for displaying API results of phase section
const phaseMessageDisp = document.getElementById("phase-message-display")

// define total number of switches
const totalSwitches = 18

// define the beginning of the URL for all API calls
const apiURLheader = "http://ubuntu.local/api/v1/"

// All set switch status toggle switch document elements in order
var setSwitches = []
for (let i = 0; i < totalSwitches; i++) {
    switchNum = (i+1)
    switchName = "swt" + switchNum.toString()
    setSwitches.push(document.getElementById(switchName))
}

// All display switch status toggle switch document elements in order
var displaySwitches = []
for (let i = 0; i < totalSwitches; i++) {
    switchNum = (i+1)
    switchName = "sw" + switchNum.toString()
    displaySwitches.push(document.getElementById(switchName))
}

// All set phase status drop-down menu document elements in order
var setPhaseMenus = []
for (let i = 0; i < totalSwitches; i++) {
    switchNum = (i+1)
    menuName = "menu" + switchNum.toString()
    setPhaseMenus.push(document.getElementById(menuName))
}

// All display phase status drop-down menu document elements in order
var displayPhaseMenus = []
for (let i = 0; i < totalSwitches; i++) {
    switchNum = (i+1)
    menuName = "dispmenu" + switchNum.toString()
    displayPhaseMenus.push(document.getElementById(menuName))
}

// Get the ZCS set and ZCS display toggle switch document elements
var zcsSetSwitch = document.getElementById("zcs")
var zcsDispSwitch = document.getElementById("zcs-display")

// Query everything and display on page when first loaded in
getSwitchStatus();
getZCSStatus();
getPhaseStatus();

// Display the current status of the switches according the the 'states' string (eg. "111111000000111111")
function displaySwitchStatus(states) {
    for (let i = 0; i < states.length; i++) {
        if (states[i] === "1") {
            displaySwitches[i].checked = true
        } else if (states[i] === "0") {
            displaySwitches[i].checked = false
        }
    };
}

// Getting the states of switch document elements and returning a string representation of states (eg, "111111000000111111")
function getSwitchesStates() {
    states = ""

    for (let i = 0; i < setSwitches.length; i++) {
        if (setSwitches[i].checked) {
            states += "1"
        } else if (!setSwitches[i].checked) {
            states += "0"
        }
    };

    return states
}

// Display the current phase status according to the 'states' string (eg. "111111222222333333")
function displayPhaseStatus(states) {
    for (let i = 0; i < states.length; i++) {
        if (states[i] === "1") {
            displayPhaseMenus[i].value = "1"
        } else if (states[i] === "2") {
            displayPhaseMenus[i].value = "2"
        } else if (states[i] === "3") {
            displayPhaseMenus[i].value = "3"
        }
    };
}

// Getting the desired state of the phases and returning a string representation of states (eg "111111222222333333")
function getPhaseStates() {
    phaseString = ""

    for (let i = 0; i < setPhaseMenus.length; i++) {
        phaseString += setPhaseMenus[i].value;
    };

    return phaseString
}

// Settting the states of all set switch status toggle switch document elements to be On
function setSwitchesAllOn() {
    for (let i = 0; i < setSwitches.length; i++) {
        setSwitches[i].checked = true
    };
    document.getElementById("switches-off").checked = false
}

// Settting the states of all set switch status toggle switch document elements to be Off
function setSwitchesAllOff() {
    for (let i = 0; i < setSwitches.length; i++) {
            setSwitches[i].checked = false
    };
    document.getElementById("switches-on").checked = false
}

// Getting the status of switches from the Load Bank
async function getSwitchStatus() {
    const url = apiURLheader + "switches/status"

    let result = await getApiResult(url);
    console.log(result)
    displaySwitchStatus(result.switches)

    switchMessageDisp.textContent = JSON.stringify(result)
    switchMessageDisp.style.color = "black";
}

// Setting the status of switches on the Load Bank
async function setSwitchStatus() {
    let values = getSwitchesStates(setSwitches)
    const url = apiURLheader + "switches?values=" + values

    let result = await getApiResult(url);

    // if we got a ZCS timeout, display that error to the screen
    // otherwise, get the switch status and have that function display result to screen
    if (result.status == "OK") {
        getSwitchStatus()
    } else {
        switchMessageDisp.textContent = JSON.stringify(result)
        switchMessageDisp.style.color = "red"
    }

    // turn off the all switches on/off radios
    document.getElementById("switches-off").checked = false
    document.getElementById("switches-on").checked = false
}

async function getZCSStatus() {
    const url = apiURLheader + "zcs/status"

    let result = await getApiResult(url);
    console.log(JSON.stringify(result));

    zcsMessageDisp.textContent = JSON.stringify(result)

    if (result.zcs == "1") {
        zcsDispSwitch.checked = true;
    } else if (result.zcs == "0") {
        zcsDispSwitch.checked = false;
    }
}

// Setting the ZCS status on the Load Bank
async function setZCSStatus() { 
	let desired_state = zcsSetSwitch.checked
	url = "";

	if (desired_state == true) {
		url = apiURLheader + "zcs/on"
	} else if (desired_state == false) {
		url = apiURLheader + "zcs/off"
	}

	let result = await getApiResult(url);

	console.log(JSON.stringify(result));
	getZCSStatus();
}

async function getPhaseStatus() {
    const url = apiURLheader + "phases/status"

    let result = await getApiResult(url);

    console.log(result);

    phaseMessageDisp.textContent = JSON.stringify(result)

    displayPhaseStatus(result.phases);
}

async function setPhaseStatus() {
    let values = getPhaseStates()
    const url = apiURLheader + "phases?values=" + values

    let result = await getApiResult(url);

    console.log(JSON.stringify(result));
    getPhaseStatus()
}

// Making a HTTP Get API call and expecting a Json result
async function getApiResult(url) {
    var requestOptions = {
      method: 'GET',
      redirect: 'follow'
    };

    try {
      const response = await fetch(url, requestOptions);
      const result = await response.json();
      return result; // This will be a JSON object
    } catch (error) {
      console.log('error', error);
      return null; // Handle the error gracefully, you can also throw an error if needed
    }
}
