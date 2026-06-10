# Charity Donation Management System — C++

A console-based C++ application that simulates a charity donation platform, developed as part of the Programming II course project at Université Antonine. The system supports user registration and login, admin and donor roles, real-time charity management, and full donation tracking — all persisted in structured JSON files.

---

## Table of Contents

- Features
- Project Structure
- Data Storage
- Getting Started
- Usage
- Input Validation
- JSON File Formats
- Technical Details
- Bonus Features
- Authors
---

## Features

### Authentication
- New user registration with full input validation
- Existing user login via email and hashed password
- Automatic unique userID generation
- Role detection: Admin vs Donor

### Admin Panel
- Add a new charity campaign (with duplicate check)
- Delete an existing charity
- Update charity details (name, description, target amount, status, etc.)

### Donor Panel
- Browse all available charity campaigns
- Make a donation by selecting a charity, entering an amount, and an optional message
- The charity's current amount is automatically updated upon each new donation
- View donation history
- Cancel a donation, which removes it from both the in-memory structure and the JSON file and adjusts the charity's current amount accordingly
- Edit a donation by changing the amount or the target charity, with charity name validation

---

## Project Structure

```
charity-donation-system/
│
├── main.cpp
├── auth.h / auth.cpp
├── admin.h / admin.cpp
├── donor.h / donor.cpp
├── fileManager.h / fileManager.cpp
├── validation.h / validation.cpp
├── structures.h
│
├── data/
│   ├── utilisateurs.json
│   ├── donations.json
│   └── charities.json
│
└── README.md
```

---

## Data Storage

All data is stored in JSON files as part of the bonus implementation:

| File              | Contents
| utilisateurs.json | User accounts: ID, name, hashed password, email, phone number 
| donations.json    | Donation records: donationID, userID, charityID, amount, date and time 
| charities.json    | Charity campaigns: charityID, name, description, target amount, current amount, deadline, status 


No data is duplicated across files. Every add, edit, or delete operation is reflected immediately in the corresponding JSON file and in the in-memory structure.

---

## Getting Started

### Prerequisites
- A C++ compiler supporting C++
- The nlohmann/json library (header-only), available at https://github.com/nlohmann/json

---

## Usage

At launch, the program asks whether the user already has an account or not.

If the user is new, they go through the registration flow and are then directed to the appropriate menu based on their role.

If the user already has an account, they log in with their email and password.

**Admin Menu**
- Add a charity
- Delete a charity
- Update charity information
- Logout

**Donor Menu**
- Browse available charities
- Make a donation
- View my donations
- Cancel a donation
- Edit a donation
- Logout

---

## Input Validation

All user input is validated before being accepted:

| Field | Validation Rule |
|---|---|
| Password | Minimum 8 characters, must include letters, digits, and special characters |
| Email | Must match standard email format |
| Phone number | Must match a valid numeric format |
| Charity name (on donation edit) | Must exist in charities.json |
| Duplicate users | Checked by email before registration |
| Duplicate charities | Checked by name before adding |

---

## Technical Details

The project is built around four main structures: Client, Charity, Donation, and Date.

The Client structure holds the user's ID, name, hashed password, contact details, number of donations, and a dynamically allocated array of Donation objects.

The Charity structure holds the charity's ID, name, description, target amount, current amount, deadline, and status.

The Donation structure holds the donation ID, the associated charity ID, the amount, and the date and time of the donation.

Key implementation highlights:
- Dynamic memory allocation using pointers for the donations array inside the Client structure
- All logic is organized into functions and procedures; main only handles the program flow
- File I/O is handled through dedicated utility functions that read from and write to JSON files
- Passwords are hashed using MD5 before being stored
- The ctime library is used for date and time handling

---

## Bonus Features

- JSON files are used instead of plain .txt files for all data storage
- The ctime library is used for date and time instead of a custom date structure

---

## Authors

| Mariam khalife | software engineer |
|---|---|
| Mariam khalife| Developer |

Université Antonine — Programming II Project, 2024–2025

---

## Academic Integrity

This project was developed as part of a graded academic assignment. Plagiarism is strictly prohibited. Any submission must represent the student's own original work.
