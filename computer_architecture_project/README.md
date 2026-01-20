# Computer Architecture Project 

---

## 📂 Folder Structure
* **sim/** - Source code for the Simulator (`.c` / `.h` files).
* **asm/** - Source code for the Assembler and `asm.exe`.
* **computer_architecture_project/** - Visual Studio 2026 Solution (`.slnx`) and Project files.
* **counter/** - Test 1: Contains inputs, outputs, and `sim.exe`.
* **mul_serial/** - Test 2: Contains inputs, outputs, and `sim.exe`.
* **mul_parralel/** - Test 3: Contains inputs, outputs, and `sim.exe`.

---

## 🛠️ How to Build
1. Open the solution file inside the `computer_architecture_project` folder.
2. Set configuration to **Release** or **Debug**.
3. Build Solution (**Ctrl+Shift+B**).

---

## 🚀 How to Run
The executables (`sim.exe`) have already been pre-built and placed in each test folder for convenience.

**To run a test:**
1. Open a terminal in the specific test folder (e.g., `counter`).
2. Run the simulator with the required arguments:
   ```bash
   .\sim.exe imem0.txt imem1.txt imem2.txt imem3.txt memin.txt memout.txt regout0.txt regout1.txt regout2.txt regout3.txt core0trace.txt core1trace.txt core2trace.txt core3trace.txt bustrace.txt dsram0.txt dsram1.txt dsram2.txt dsram3.txt tsram0.txt tsram1.txt tsram2.txt tsram3.txt stats0.txt stats1.txt stats2.txt stats3.txt