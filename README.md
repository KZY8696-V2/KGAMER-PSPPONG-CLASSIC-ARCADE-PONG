# 🏓 KGAMER Pong (PSP Homebrew)

A custom, open-source 2D **Pong** arcade game compiled natively for the **PlayStation Portable (PSP)** system using **PSPSDK**.

---

## 🌟 Features

* **Classic & Custom Gameplay:** Smooth 60 FPS 2D Pong physics tailored specifically for the PSP display.
* **5 Difficulty Modes:** Choose from **Ultra Easy**, **Easy**, **Medium**, **Hard**, or fully customizable **Custom** modes.
* **Automatic Score Logging:** **ALL** game modes, match results, and custom scores are automatically generated and saved to `scores.properties` with built-in history search capabilities.
* **Native C Performance:** Optimized low-level code written in C using the official **PSPSDK**.

---

## 🎯 Difficulty Levels

* 🟢 **Ultra Easy:** Ideal for beginners and casual play.
* 🟡 **Easy:** Light challenge with comfortable AI speeds.
* 🟠 **Medium:** Standard balanced Pong difficulty.
* 🔴 **Hard:** Fast-paced, high-speed reaction test against an aggressive AI.
* ⚙️ **Custom:** Fully configurable match parameters for personalized gameplay.

> 📝 **Score System:** Every single match played across **all** difficulty modes automatically generates and logs stats and scores to `scores.properties`!

---

## 🎮 Controls

| Button | Menu Action | In-Game Action |
| :--- | :--- | :--- |
| **D-Pad Up / Down** | Navigate Menu | Move Paddle Up / Down |
| **Cross (X)** | Select / Confirm | *(Disabled in-match)* |
| **Triangle (△)** | Back to Main Menu | *(Disabled in-match)* |
| **Select** | *(Disabled in menu)* | Return to Main Menu |

---

## 🚀 How to Install

1. Download the latest `EBOOT.PBP` build from the **Releases** section (or from the Retro Arcade Bundle).
2. Connect your PSP to your PC using a USB cable or Memory Stick card reader.
3. Open your Memory Stick directory and navigate to `PSP/GAME/`.
4. Copy `EBOOT.PBP` directly into your game folder inside `PSP/GAME/`:
   ```text
   PSP/GAME/EXAPLW
