<template>
  <div class="container">
    <div class="top-right-buttons">
      <button @click="goToDashboard" class="dashboard-button">DASHBOARD</button>
      <button @click="logout" class="logout-button">LOGOUT</button>
    </div>

    <div class="status-container">
      <div class="status"><div class="status-box available"></div> Available</div>
      <div class="status"><div class="status-box occupied"></div> Occupied</div>
    </div>

    <div class="counter-container">
      <span>Current Car Count: {{ inCarCount }}</span>
    </div>

    <div class="parking-layout">
      <div v-for="(row, rowIndex) in parkingRows" :key="rowIndex" class="parking-row">
        <draggable v-model="parkingRows[rowIndex]" group="parking-group" item-key="id" class="parking-row">
          <template #item="{ element }">
            <div v-if="element.type === 'lane'"
              :class="['parking-slot', element.type, { 'lane-block': isLaneBlock(rowIndex, element), occupied: element.occupied, available: !element.occupied }, 'rotated-' + element.rotated]">
              <span class="arrow">➡️</span>
            </div>
            <div v-else
              :class="['parking-slot', element.type, { occupied: element.occupied, available: !element.occupied }]">
              {{ element.name }}
            </div>
          </template>
        </draggable>
      </div>
    </div>

    <div class="config-panel">
      <div class="config-tools">
        <button @click="addNewRow" class="config-button">➕ Add Row</button>
        <button @click="addParkingSlot" class="config-button">➕ Add Parking</button>
        <button @click="addLane" class="config-button">➡️ Add Lane</button>
        <button @click="addEntrance" class="config-button entrance">🚗 Entrance</button>
        <button @click="addExit" class="config-button exit">🚪 Exit</button>
        <button @click="removeRow" class="config-button">❌ Remove Row</button>
        <button @click="removeLastElement" class="config-button">🗑️ Remove Last Element</button>
        <button @click="publishLayout" class="config-button">🔄 Publish Layout</button>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, watch } from 'vue'
import { useRouter } from 'vue-router'
import mqtt from 'mqtt'
import draggable from 'vuedraggable'
import { auth } from '../firebaseConfig'
import { onAuthStateChanged } from 'firebase/auth'

const router = useRouter()
const client = mqtt.connect('wss://test.mosquitto.org:8081')

const ir9Count = ref(0), ir10Count = ref(0), inCarCount = ref(0)
let lastIR9Time = 0, lastIR10Time = 0
const debounceMs = 1000
const ir9Ready = ref(true), ir10Ready = ref(true)

function handleSensor(msg, sensorNumber) {
  const now = Date.now()
  if (sensorNumber === 9) {
    if (msg === "1" && ir9Ready.value && now - lastIR9Time > debounceMs) {
      ir9Count.value++
      ir9Ready.value = false
      lastIR9Time = now
      updateInCarCount()
    }
    if (msg === "0") ir9Ready.value = true
  }
  if (sensorNumber === 10) {
    if (msg === "1" && ir10Ready.value && now - lastIR10Time > debounceMs) {
      ir10Count.value++
      ir10Ready.value = false
      lastIR10Time = now
      updateInCarCount()
    }
    if (msg === "0") ir10Ready.value = true
  }
}

const defaultLayout = [
  [
    { id: 1, name: "P1", topic: "kmitl/project/irsensor/1", occupied: false, type: 'parking' },
    { id: 2, name: "P2", topic: "kmitl/project/irsensor/2", occupied: false, type: 'parking' },
    { id: 3, name: "P3", topic: "kmitl/project/irsensor/3", occupied: false, type: 'parking' },
    { id: 4, name: "P4", topic: "kmitl/project/irsensor/4", occupied: false, type: 'parking' }
  ],
  [
    { id: "entrance", name: "Entrance", occupied: false, type: 'entrance' },
    { id: "lane", name: "Lane", occupied: false, type: 'lane', rotated: 0 },
    { id: "exit", name: "Exit", occupied: false, type: 'exit' }
  ],
  [
    { id: 5, name: "P5", topic: "kmitl/project/irsensor/5", occupied: false, type: 'parking' },
    { id: 6, name: "P6", topic: "kmitl/project/irsensor/6", occupied: false, type: 'parking' },
    { id: 7, name: "P7", topic: "kmitl/project/irsensor/7", occupied: false, type: 'parking' },
    { id: 8, name: "P8", topic: "kmitl/project/irsensor/8", occupied: false, type: 'parking' },
  ]
]

const parkingRows = ref(JSON.parse(localStorage.getItem("parkingLayout")) || defaultLayout)

watch(parkingRows, val => {
  localStorage.setItem("parkingLayout", JSON.stringify(val))
}, { deep: true })

const savedCarCount = localStorage.getItem("inCarCount")
if (savedCarCount !== null) {
  inCarCount.value = parseInt(savedCarCount)
}

onMounted(() => {
  onAuthStateChanged(auth, (user) => {
    if (!user) {
      router.push("/login")
    } else {
      client.on("connect", () => {
        parkingRows.value.flat().forEach(slot => slot.topic && client.subscribe(slot.topic))
        client.subscribe("kmitl/project/irsensor/entrance")
        client.subscribe("kmitl/project/irsensor/exit")
        client.publish("kmitl/project/carcount", inCarCount.value.toString(), { retain: true })
        publishLayout()
      })

      client.on("message", (topic, message) => {
        const msg = message.toString().trim()

        if (topic === "kmitl/project/irsensor/entrance") return handleSensor(msg, 9)
        if (topic === "kmitl/project/irsensor/exit") return handleSensor(msg, 10)

        const id = parseInt(topic.split("/").pop())
        const sensor = parkingRows.value.flat().find(s => s.topic?.endsWith(id))
        if (sensor) sensor.occupied = msg === "1"
      })
    }
  })
})

function updateInCarCount() {
  const count = Math.max(0, ir9Count.value - ir10Count.value)
  inCarCount.value = count
  localStorage.setItem("inCarCount", count)
  client.publish("kmitl/project/carcount", count.toString(), { retain: true })
}

function logout() { auth.signOut().then(() => router.push("/login")) }
function goToDashboard() { router.push("/dashboard") }
function publishLayout() { client.publish("kmitl/project/parking-layout", JSON.stringify(parkingRows.value), { retain: true }) }
function addNewRow() { parkingRows.value.push([]); publishLayout() }
function removeRow() { if (parkingRows.value.length > 1) parkingRows.value.pop(); publishLayout() }
function addParkingSlot() {
  const all = parkingRows.value.flat()
  const nums = all.filter(s => s.type === 'parking' && s.name.startsWith('P')).map(s => parseInt(s.name.slice(1)))
  const next = nums.length ? Math.max(...nums) + 1 : 1
  const topic = kmitl/project/irsensor/${next}
  const slot = {
    id: next,
    name: `P${next}`,
    topic: topic,
    occupied: false,
    type: 'parking'
  }
  parkingRows.value.at(-1).push(slot)
  client.subscribe(topic)  // Subscribe ใหม่หลังจากเพิ่ม slot
  publishLayout()
}


function addEntrance() { parkingRows.value[1].unshift({ id: "entrance", name: "Entrance", occupied: false, type: 'entrance' }); publishLayout() }
function addExit() { parkingRows.value[1].push({ id: "exit", name: "Exit", occupied: false, type: 'exit' }); publishLayout() }
function addLane() {
  parkingRows.value[0].splice(1, 0, {
    id: `lane-${Date.now()}`,
    name: "Lane",
    occupied: false,
    type: 'lane',
    rotated: 0
  });
  publishLayout();
}

function removeLastElement() {
  for (let i = parkingRows.value.length - 1; i >= 0; i--) {
    if (parkingRows.value[i].length > 0) {
      parkingRows.value[i].pop()
      publishLayout()
      break
    }
  }
}
function toggleLaneDirection(id) {
  const lane = parkingRows.value.flat().find(s => s.id === id && s.type === 'lane')
  if (lane) { lane.rotated = (lane.rotated + 1) % 4; publishLayout() }
}
function isLaneBlock(rowIndex, element) {
  const row = parkingRows.value[rowIndex]
  const index = row.indexOf(element)
  return index > 0 && row[index - 1].type === 'lane'
}
</script>

<style scoped>
/* Responsive Layout for parking slots and rows */
.parking-layout {
  width: 100%;
  overflow-x: auto;
  padding: 10px 0;
}
.parking-row {
  display: flex;
  gap: 10px;
  justify-content: flex-start;
  padding: 5px 0;
}

/* Slot Styling */
.parking-slot, .lane, .entrance, .exit {
  flex-shrink: 0;
}

.container {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  min-height: 100vh;
  background: linear-gradient(to right, #3a7bd5, #3a6073);
  padding: 20px;
  color: white;
  width: 100%;
  max-width: 100%;
}

.top-right-buttons {
  position: absolute;
  top: 20px;
  right: 20px;
  display: flex;
  gap: 10px;
}

/* Rest of the styles from before remain unchanged */
</style>


<style scoped>
/* ย่อความยาว: ใช้ style เดิมทั้งหมด เช่นเดียวกับในไฟล์ก่อนหน้า
   รวมถึง media query สำหรับ mobile ที่ responsive */
</style>


<style scoped>
/* Responsive for layout rows */
.parking-layout {
  width: 100%;
  overflow-x: auto;
  padding: 10px 0;
}
.parking-row {
  flex-wrap: nowrap;
  overflow-x: auto;
  padding-bottom: 10px;
}

/* Responsive for parking slots */
.parking-slot,
.lane,
.entrance,
.exit {
  flex-shrink: 0;
}

.container {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  min-height: 100vh;
  background: linear-gradient(to right, #3a7bd5, #3a6073);
  padding: 20px;
  color: white;
  width: 100%;
  max-width: 100%;
  overflow-x: hidden;

}
.top-right-buttons {
  position: absolute;
  top: 20px;
  right: 20px;
  display: flex;
  gap: 10px;
}
.logout-button {
  background: linear-gradient(to right, #ff5733, #ff8d72);
  color: black;
  padding: 8px 12px;
  border-radius: 8px;
  cursor: pointer;
}
.dashboard-button {
  background: linear-gradient(to right, #b2f7ef, #7b8df2);
  color: black;
  padding: 8px 12px;
  border-radius: 8px;
  cursor: pointer;
}
.logout-button:hover,
.dashboard-button:hover {
  opacity: 0.8;
}
.status-container {
  position: absolute;
  top: 20px;
  left: 20px;
  display: flex;
  flex-direction: column;
  gap: 10px;
  background: rgba(0, 0, 0, 0.7);
  padding: 12px 18px;
  border-radius: 10px;
}
.status
