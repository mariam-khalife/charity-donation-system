#include <iostream>
#include<iomanip>
#include<ctime>
#include <fstream>
#include <set>
#include <algorithm>
#include <string>
#include <cstdio>
#include <nlohmann/json.hpp>
#include <cryptopp/sha.h>
#include <cryptopp/hex.h>
#include <cryptopp/filters.h>
#include <cryptopp/files.h>
#include <conio.h> 

using namespace std;
using json = nlohmann::json;
using namespace CryptoPP;

struct donation {
	int donationID;
	int charityID;
	double amount;
	string date;
	string time;
};

struct User {
	int userID;
	string firstName;
	string lastName;
	string password;
	string phone;
	string email;
	int nb_donations;
	donation* donations;
};

struct charity {
	int charityID;
	string name;
	string description;
	double targetAmount;
	double currentAmount;
	tm d;
	string status;
};

// Fonctions de validation
bool isValidName(const string& name) {
	return all_of(name.begin(), name.end(), [](char c) {
		return isalpha(c) || c == '-' || c == ' ';
		});
}
bool isPasswordValid(const string& pass) {
	bool hasUpper = false, hasLower = false, hasDigit = false, hasSpecial = false;
	if (pass.length() < 8) return false;

	for (char c : pass) {
		if (isupper(c)) hasUpper = true;
		else if (islower(c)) hasLower = true;
		else if (isdigit(c)) hasDigit = true;
		else if (!isspace(c)) hasSpecial = true;
	}

	return hasUpper && hasLower && hasDigit && hasSpecial;
}
bool isEmailValid(const string& email) {
	size_t at = email.find('@');
	if (at == string::npos || at == 0 || at == email.length() - 1) { return false; }

	size_t dot = email.find('.', at);
	return dot != string::npos && dot > at + 1 && dot < email.length() - 1;
}
bool isPhoneValid(const string& phone) {
	// Règles :
	// 1. 8 chiffres minimum
	// 2. Max  2 '-' ou espaces
	// 3. Pas de separateurs consecutifs
	// 4. Formats connus (ex: xx-xxxxxx, xx xxx xxx)

	int digits = 0, separators = 0;
	bool hasPlus = false;

	for (size_t i = 0; i < phone.length(); i++) {
		char c = phone[i];
		if (isdigit(c)) digits++;
		else if (c == '-' || c == ' ') {
			if (i == 0 || i == phone.length() - 1) return false; 
			if (phone[i - 1] == '-' || phone[i - 1] == ' ') return false; // Pas consecutifs
			separators++;
		}
		else return false; // Caractère invalide
	}

	if (digits < 8) return false;
	if (separators > 2) return false;

	return true;
}

string centerAlign(const string& text, int width) {
	if (text.length() >= width) return text;

	int padding = (width - text.length()) / 2;
	return string(padding, ' ') + text + string(padding, ' ');
}

// Verifie si l'email est unique dans la base
bool isEmailUnique(const string& email, const json& clientsData) {
	if (!clientsData.contains("clients") || !clientsData["clients"].is_array()) {
		return true; // Si pas de donnees clients, considere comme unique
	}

	for (const auto& client : clientsData["clients"]) {
		if (client.contains("email") && client["email"] == email) {
			return false;
		}
	}
	return true;
}
bool isPhoneUnique(const string& phone, const json& clientsData) {
	// Verifie si la structure JSON contient bien un tableau "clients"
	if (!clientsData.contains("clients") || !clientsData["clients"].is_array()) {
		return true; // Si pas de donnees clients, le numero est considere comme unique
	}

	// Parcourt tous les clients pour verifier le numero
	for (const auto& client : clientsData["clients"]) {
		// Verifie que l'objet client contient bien un champ "phone" avant comparaison
		if (client.contains("phone") && client["phone"] == phone) {
			return false; // Numero trouve -> pas unique
		}
	}
	return true; // Numero non trouve -> unique
}
bool isNameUnique(const string& firstName, const string& lastName, const json& clientsData) {
	// Verifie si la structure JSON est valide
	if (!clientsData.is_object() || !clientsData.contains("clients") || !clientsData["clients"].is_array()) {
		return true;
	}

	// Fonction lambda pour convertir en minuscules
	auto toLower = [](string str) {
		transform(str.begin(), str.end(), str.begin(), ::tolower);
		return str;
		};

	string searchFirst = toLower(firstName);
	string searchLast = toLower(lastName);

	// Parcourt tous les clients
	for (const auto& client : clientsData["clients"]) {
		if (client.is_object() && client.contains("firstName") && client.contains("lastName")) {
			if (toLower(client["firstName"]) == searchFirst &&
				toLower(client["lastName"]) == searchLast) {
				return false; // Doublon trouve
			}
		}
	}
	return true; // Aucun doublon trouve
}

bool isValidDateTime(const string& datetimeStr, tm& result) {

	if (datetimeStr.length() != 16) return false;

	int day, month, year, hour, minute;
	char sep1, sep2, comma, sep3;

	istringstream iss(datetimeStr);
	iss >> day >> sep1 >> month >> sep2 >> year >> comma >> hour >> sep3 >> minute;

	if (iss.fail() || sep1 != '/' || sep2 != '/' || comma != ',' || sep3 != ':') {
		return false;
	}


	if (month < 1 || month > 12 || day < 1 || day > 31 ||
		hour < 0 || hour > 23 || minute < 0 || minute > 59) {
		return false;
	}

	// Conversion en struct tm
	result = {};
	result.tm_mday = day;
	result.tm_mon = month - 1;
	result.tm_year = year - 1900;
	result.tm_hour = hour;
	result.tm_min = minute;
	result.tm_isdst = -1;

	// Validation finale
	time_t test = mktime(&result);
	return test != -1;
}
string getCurrentDate() {
	time_t now = time(nullptr);
	tm localTime;

	if (localtime_s(&localTime, &now) != 0) {
		return "01/01/1970"; // Retourne une date par defaut en cas d'erreur
	}

	char buffer[11]; // Suffisant pour "JJ/MM/AAAA\0"
	snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d",
		localTime.tm_mday,
		localTime.tm_mon + 1,
		localTime.tm_year + 1900);

	return string(buffer);
}
string getCurrentTime() {
	time_t now = time(nullptr);
	tm localTime;

	// Version securisee avec localtime_s
	if (localtime_s(&localTime, &now) != 0) {
		return "00:00"; // Retourne une heure par defaut en cas d'erreur
	}

	char buffer[6]; // Suffisant pour "HH:MM\0"
	snprintf(buffer, sizeof(buffer), "%02d:%02d",
		localTime.tm_hour,
		localTime.tm_min);

	return string(buffer);
}

// Fonction unique pour lire tous les IDs existants
int getNextID(const string& filename, const string& arrayKey, const string& idKey) {
	set<int> existingIDs;
	ifstream file(filename);

	if (file) {
		try {
			json data;
			file >> data;

			// Verifie si la cle du tableau existe
			if (data.contains(arrayKey)) {
				// Parcourt tous les elements du tableau
				for (const auto& item : data[arrayKey]) {
					if (item.contains(idKey)) {
						existingIDs.insert(item[idKey].get<int>());
					}
				}
			}
		}
		catch (...) {
			// En cas d'erreur de lecture, on continue avec un set vide
		}
	}
	file.close();

	// Si aucun ID existe, commencer a 1
	if (existingIDs.empty()) {
		return 1;
	}

	// Trouver le premier trou dans la sequence 
	int expectedID = 1;
	for (int id : existingIDs) {
		if (id > expectedID) {
			// Il y a un trou avant cet ID
			return expectedID;
		}
		expectedID = id + 1;
	}

	// Si aucun trou, retourner le prochain ID après le maximum
	return expectedID;
}

// Fonction pour charger les donnees 
json loadutilisateursData(const string& filename) {
	json utilisateursData;
	ifstream inFile(filename);

	// si fichier n'existe pas ou est vide
	if (!inFile.is_open()) {
		cout << "Fichier " << filename << " non trouve. Creation d'une nouvelle structure." << endl;
		utilisateursData["utilisateurs"] = json::array();
		return utilisateursData;
	}

	// Verifier si le fichier est vide
	if (inFile.peek() == ifstream::traits_type::eof()) {
		cout << "Fichier " << filename << " vide. Initialisation d'une nouvelle structure." << endl;
		utilisateursData["utilisateurs"] = json::array();
		inFile.close();
		return utilisateursData;
	}

	try {
		// Tentative de lecture du fichier JSON
		inFile >> utilisateursData;

		// Validation de la structure des donnees
		if (!utilisateursData.is_object()) {
			throw runtime_error("Format JSON invalide: la racine doit être un objet");
		}

		if (!utilisateursData.contains("utilisateurs")) {
			cout << "Aucun tableau 'utilisateurs' trouve. Initialisation." << endl;
			utilisateursData["utilisateurs"] = json::array();
		}
		else if (!utilisateursData["utilisateurs"].is_array()) {
			throw runtime_error("Le champ 'utilisateurs' doit être un tableau");
		}

		// Validation optionnelle de chaque utilisateur
		for (auto& user : utilisateursData["utilisateurs"]) {
			if (!user.is_object()) {
				cout << "Avertissement: element invalide dans le tableau utilisateurs" << endl;
				continue;
			}

			// Verification des champs obligatoires
			vector<string> requiredFields = { "userID", "firstName", "lastName", "password", "email", "phone" };
			for (const auto& field : requiredFields) {
				if (!user.contains(field)) {
					cout << "Avertissement: utilisateur sans champ '" << field << "'" << endl;
				}
			}
		}

	}
	catch (const json::parse_error& e) {
		cerr << "Erreur de parsing JSON: " << e.what() << endl;
		cerr << "Initialisation d'une nouvelle structure de donnees." << endl;
		utilisateursData = json::object();
		utilisateursData["utilisateurs"] = json::array();
	}
	catch (const exception& e) {
		cerr << "Erreur: " << e.what() << endl;
		utilisateursData = json::object();
		utilisateursData["utilisateurs"] = json::array();
	}

	inFile.close();
	return utilisateursData;
}
json loadDonationsData(const string& filename = "donations.json") {
	json donationsData;
	ifstream inFile(filename);

	if (inFile.good()) {
		try {
			inFile >> donationsData;
		}
		catch (...) {
			// If file exists but is corrupted, initialize with empty structure
			donationsData = { {"donations", json::array()} };
		}
	}
	else {
		// If file doesn't exist, initialize with empty structure
		donationsData = { {"donations", json::array()} };
	}

	return donationsData;
}
json loadOrCreateCharitiesFile(const string& filename = "charities.json") {
	json charitiesData;

	try {
		// 1. Tentative d'ouverture du fichier
		ifstream inFile(filename);

		// 2. Verification si le fichier existe et n'est pas vide
		if (inFile.good() && inFile.peek() != ifstream::traits_type::eof()) {
			try {
				// 3. Lecture du fichier JSON
				inFile >> charitiesData;

				// 4. Validation de la structure
				if (!charitiesData.is_object() || !charitiesData.contains("charities")) {
					throw runtime_error("Structure JSON invalide - 'charities' manquant");
				}

				if (!charitiesData["charities"].is_array()) {
					throw runtime_error("Le champ 'charities' doit être un tableau");
				}

				// 5. Validation optionnelle de chaque charite
				for (auto& charity : charitiesData["charities"]) {
					if (!charity.is_object()) {
						cerr << "Avertissement: element invalide dans le tableau des charites" << endl;
						continue;
					}

					// Verification des champs obligatoires
					vector<string> requiredFields = { "charityID", "name", "description",
													"targetAmount", "currentAmount", "status" };
					for (const auto& field : requiredFields) {
						if (!charity.contains(field)) {
							cerr << "Avertissement: Charite sans champ '" << field << "'" << endl;
						}
					}
				}
			}
			catch (const json::parse_error& e) {
				cerr << "Erreur de parsing JSON: " << e.what() << endl;
				cerr << "Recreation d'une structure valide..." << endl;
				charitiesData = json::object();
				charitiesData["charities"] = json::array();
			}
		}
		else {
			// Fichier inexistant ou vide - creation d'une structure vide
			charitiesData["charities"] = json::array();
		}

		inFile.close();

		// 6. Sauvegarde initiale si necessaire
		if (charitiesData["charities"].empty()) {
			ofstream outFile(filename);
			if (outFile.good()) {
				outFile << charitiesData.dump(4);
				cout << "Fichier " << filename << " initialise avec une structure vide." << endl;
			}
			outFile.close();
		}

	}
	catch (const exception& e) {
		cerr << "Erreur critique: " << e.what() << endl;
		// Structure de secours
		charitiesData = json::object();
		charitiesData["charities"] = json::array();
	}

	return charitiesData;
}

// Fonction pour hacher le mot de passe en SHA-256
string Gethash(const string& password) {
	/*try {
		SHA256 hash;
		string digest;

		StringSource s(password, true,
			new HashFilter(hash,
				new HexEncoder(
					new StringSink(digest)
				)
			)
		);
		return digest;
	}
	catch (const CryptoPP::Exception& e) {
		cerr << "Erreur CryptoPP: " << e.what() << endl;
		return "";
	}*/
	return password;
}
unsigned long hashString(const string& str) {
	unsigned long hash = 5381;
	int c;

	for (size_t i = 0; i < str.length(); ++i) {
		c = str[i];
		hash = ((hash << 5) + hash) + c; // hash * 33 + c
	}

	return hash;
}
string MaskedPassword() {
	string password;
	char ch;
	while ((ch = _getch()) != '\r') { // '\r' = Entrée
		if (ch == '\b') { // Backspace
			if (!password.empty()) {
				cout << "\b \b"; // efface un caractère
				password.pop_back();
			}
		}
		else {
			password += ch;
			cout << '*';
		}
	}
	cout << endl;
	return password;
}


User* createNewUser() {
	User* newUser = new User();
	bool isValid = false;
	auto utilisateursData = loadutilisateursData("utilisateurs.json");

	cout << "\n--- Creation d'un nouveau compte ---" << endl;

	// Saisie du prenom et nom avec verification d'unicite
	do {
		cout << "Entrez votre prenom: ";
		getline(cin, newUser->firstName);

		cout << "Entrez votre nom: ";
		getline(cin, newUser->lastName);

		if (!isValidName(newUser->firstName) || !isValidName(newUser->lastName)) {
			cout << "Erreur: Le nom et prenom ne doivent contenir que des lettres, espaces ou tirets\n";
		}
		else if (!isNameUnique(newUser->firstName, newUser->lastName, utilisateursData)) {
			cout << "Erreur: Un utilisateur avec ce nom et prenom existe deja.\n";
			cout << "Veuillez utiliser un nom complet different ou vous connecter.\n\n";
		}
		else {
			isValid = true;
		}
	} while (!isValid);
	isValid = false;

	// Saisie du mot de passe
	string passwordConfirm;
	do {
		do {
			cout << "Creez un mot de passe (min 8 caractères, majuscule, minuscule, chiffre, special): ";
			//getline(cin, newUser->password);
			newUser->password = MaskedPassword();

			if (!isPasswordValid(newUser->password)) {
				cout << "Le mot de passe doit contenir:\n";
				cout << "- 8 caractères minimum\n";
				cout << "- Au moins une majuscule\n";
				cout << "- Au moins une minuscule\n";
				cout << "- Au moins un chiffre\n";
				cout << "- Au moins un caractère special\n";
			}
			else {
				isValid = true;
			}
		} while (!isValid);
		isValid = false;

		cout << "Confirmez votre mot de passe: ";
		//getline(cin, passwordConfirm);
		passwordConfirm = MaskedPassword();



		if (newUser->password != passwordConfirm) {
			cout << "Erreur: Les mots de passe ne correspondent pas. Veuillez recommencer.\n";
		}
		else {
			isValid = true;
		}
	} while (!isValid);
	isValid = false;

	// Saisie de l'email
	do {
		cout << "Entrez votre email: ";
		getline(cin, newUser->email);

		if (!isEmailValid(newUser->email)) {
			cout << "Format d'email invalide. Exemple valide: exemple@domaine.com\n";
		}
		else if (!isEmailUnique(newUser->email, utilisateursData)) {
			cout << "Cet email est deja utilise. Veuillez en choisir un autre.\n";
		}
		else {
			isValid = true;
		}
	} while (!isValid);
	isValid = false;

	// Saisie du telephone
	do {
		cout << "Entrez votre numero de telephone: ";
		getline(cin, newUser->phone);

		if (!isPhoneValid(newUser->phone)) {
			cout << "Numero invalide. Formats acceptes :\n"
				<< "- 8 chiffres (ex: 70123456)\n"
				<< "- xx-xxxxxx (ex: 03-123456)\n"
				<< "- xx xxx xxx (ex: 03 123 456)\n"
				<< "Veuillez reessayer : ";
		}
		else if (!isPhoneUnique(newUser->phone, utilisateursData)) {
			cout << "Ce numero de telephone est deja utilise par un autre utilisateur. Veuillez en choisir un autre.\n";
		}
		else {
			isValid = true;
		}
	} while (!isValid);

	return newUser;
}

// Fonction pour convertir tm en string au format "DD/MM/YYYY, HH:MM"
string tmToString(const tm& timeStruct) {
	ostringstream oss;
	oss << setfill('0')
		<< setw(2) << timeStruct.tm_mday << "/"
		<< setw(2) << (timeStruct.tm_mon + 1) << "/"
		<< setw(4) << (timeStruct.tm_year + 1900) << ", "
		<< setw(2) << timeStruct.tm_hour << ":"
		<< setw(2) << timeStruct.tm_min;
	return oss.str();
}

void displayUser(const User* user) {
	// Afficher les informations sous forme de tableau avant sauvegarde
	const int COL1_WIDTH = 20;
	const int COL2_WIDTH = 50;
	const int TOTAL_WIDTH = COL1_WIDTH + COL2_WIDTH;

	cout << "\n" << string(TOTAL_WIDTH, '=') << "\n";
	cout << "\nINFORMATIONS UTILISATEUR A SAUVEGARDER\n";
	cout << "=====================================\n";
	cout << left
		<< setw(COL1_WIDTH) << "CHAMP"
		<< setw(COL2_WIDTH) << "VALEUR"
		<< "\n";
	cout << string(TOTAL_WIDTH, '-') << "\n";

	cout << setw(COL1_WIDTH) << "ID"
		<< setw(COL2_WIDTH) << user->userID << "\n";
	cout << setw(COL1_WIDTH) << "Prenom"
		<< setw(COL2_WIDTH) << user->firstName << "\n";
	cout << setw(COL1_WIDTH) << "Nom"
		<< setw(COL2_WIDTH) << user->lastName << "\n";
	cout << setw(COL1_WIDTH) << "Email"
		<< setw(COL2_WIDTH) << user->email << "\n";
	cout << setw(COL1_WIDTH) << "Telephone"
		<< setw(COL2_WIDTH) << user->phone << "\n";

	cout << string(TOTAL_WIDTH, '=') << "\n\n";

}
void display1Charity(const charity* ch) {
	// Configuration des dimensions du tableau
	const int COL1_WIDTH = 20;
	const int COL2_WIDTH = 50;
	const int TOTAL_WIDTH = COL1_WIDTH + COL2_WIDTH;
	const string SEP_LINE(TOTAL_WIDTH, '=');
	const string HEADER_LINE(TOTAL_WIDTH, '-');

	
	auto formatAmount = [](double amount) {
		stringstream ss;
		ss << fixed << setprecision(2) << amount << " $";
		return ss.str();
		};

	
	auto truncateText = [](const string& text, size_t maxLength) {
		return text.length() > maxLength ? text.substr(0, maxLength - 3) + "..." : text;
		};


	cout << "\n" << SEP_LINE << "\n"
		<< centerAlign("RECAPITULATIF DE LA CHARITE", TOTAL_WIDTH) << "\n"
		<< SEP_LINE << "\n";

	cout << left
		<< setw(COL1_WIDTH) << "ID:"
		<< setw(COL2_WIDTH) << ch->charityID << "\n"

		<< setw(COL1_WIDTH) << "Nom:"
		<< setw(COL2_WIDTH) << "\"" + ch->name + "\"" << "\n"

		<< setw(COL1_WIDTH) << "Description:"
		<< setw(COL2_WIDTH) << "\"" + truncateText(ch->description, 47) + "\"" << "\n"

		<< setw(COL1_WIDTH) << "Montant cible:"
		<< setw(COL2_WIDTH) << formatAmount(ch->targetAmount) << "\n"

		<< setw(COL1_WIDTH) << "Montant collecte:"
		<< setw(COL2_WIDTH) << formatAmount(ch->currentAmount) << "\n"

		<< setw(COL1_WIDTH) << "Date limite:"
		<< setw(COL2_WIDTH) << "\"" + tmToString(ch->d) + "\"" << "\n"

		<< setw(COL1_WIDTH) << "Statut:"
		<< setw(COL2_WIDTH) << "\"" + ch->status + "\"" << "\n"

		<< SEP_LINE << "\n\n";

	// Affichage complementaire si la description est tronquee
	if (ch->description.length() > 47) {
		cout << "Note: Description tronquee. Longueur totale: "
			<< ch->description.length() << " caractères.\n\n";
	}
}
// Fonction pour afficher toutes les charites avec un formatage tableau
void displayTousCharities(const json& charitiesData) {
	if (charitiesData["charities"].empty()) {
		cout << "\nAucune charite enregistree.\n";
		return;
	}
	cout << "\n" << string(120, '=') << "\n";
	cout << "\n=== LISTE DES CHARITeS ===" << charitiesData["charities"].size() << ")\n";
	cout << left
		<< setw(10) << "ID"
		<< setw(30) << "NOM"
		<< setw(50) << "DESCRIPTION"
		<< setw(15) << "CIBLE"
		<< setw(15) << "ACTUEL"
		<< setw(25) << "DATE LIMITE"
		<< setw(15) << "STATUT"
		<< "\n";

	cout << string(120, '-') << "\n";

	// Affichage des donnees
	for (const auto& charity : charitiesData["charities"]) {
		// Formatage des donnees longues
		string desc = charity["description"];
		if (desc.length() > 48) desc = desc.substr(0, 45) + "...";

		string name = charity["name"];
		if (name.length() > 28) name = name.substr(0, 25) + "...";

		// Affichage de la ligne
		cout << left
			<< setw(10) << charity["charityID"].get<int>()
			<< setw(30) << name
			<< setw(50) << desc
			<< setw(15) << fixed << setprecision(2) << charity["targetAmount"].get<double>()
			<< setw(15) << fixed << setprecision(2) << charity["currentAmount"].get<double>()
			<< setw(25) << charity["deadline"].get<string>()
			<< setw(15) << charity["status"].get<string>()
			<< "\n";
	}
	cout << string(120, '=') << "\n\n";
}


// Fonction pour determiner le type d'utilisateur
const string ADMIN_ENTER_KEY = "Admin@@11";
User* authenticateAdmin() {
	string adminPass;
	cout << "Entrez le mot de passe admin : ";
	cin >> adminPass;
	cin.ignore();

	if (adminPass == ADMIN_ENTER_KEY) {
		// Cree un utilisateur admin temporaire
		User* admin = new User();
		admin->userID = -1;  // ID special pour admin
		admin->firstName = "Admin";
		admin->lastName = "System";
		return admin;
	}
	return nullptr;
}
string UserType(User*& user) {
	string type;
	cout << "Etes-vous un administrateur ou un donateur? (admin/donateur): ";
	cin >> type;

	// Convertir en minuscules pour la comparaison
	transform(type.begin(), type.end(), type.begin(), ::tolower);

	if (type == "admin") {
		User* admin = authenticateAdmin();
		if (admin) {
			user = admin; 
			return "admin";
		}
		else {
			cout << "Mot de passe admin incorrect. Vous serez traité comme donateur.\n";
			return "donateur";
		}
	}
	return "donateur";
}

User* loginUser() {
	json utilisateursData;

	try {
		ifstream inFile("utilisateurs.json");
		if (!inFile.is_open()) {
			cout << "Aucun fichier utilisateur trouve. Veuillez creer un compte." << endl;
			return createNewUser();  // Retourne directement un nouvel utilisateur
		}

		// Verifier si vide
		if (inFile.peek() == ifstream::traits_type::eof()) {
			cout << "Fichier vide. Creation d'un nouveau compte." << endl;
			inFile.close();
			return createNewUser();
		}

		inFile >> utilisateursData;
		inFile.close();
	}
	catch (...) {
		cout << "Erreur de lecture du fichier. Creation d'un nouveau compte." << endl;
		return createNewUser();
	}

	// Si  vide ou mal formate
	if (!utilisateursData.contains("utilisateurs") || !utilisateursData["utilisateurs"].is_array()) {
		cout << "Aucun utilisateur enregistre ou format invalide. Veuillez creer un compte." << endl;
		return createNewUser();
	}

	int attempts = 3;
	while (attempts > 0) {
		cout << "\n=== Connexion Utilisateur ===" << endl;
		cout << "Tentatives restantes: " << attempts << endl;

		string firstName, lastName, password;
		cout << "Prenom: ";
		getline(cin, firstName);
		cout << "Nom: ";
		getline(cin, lastName);
		cout << "Mot de passe: ";
		//getline(cin, password);
		password = MaskedPassword();


		// Convertir en minuscules pour comparaison
		auto toLower = [](string str) {
			transform(str.begin(), str.end(), str.begin(), ::tolower);
			return str;
			};

		// Recherche de l'utilisateur
		for (const auto& user : utilisateursData["utilisateurs"]) {
			try {
				string storedFirst = toLower(user["firstName"]);
				string storedLast = toLower(user["lastName"]);
				string storedPass = user["password"];


				if (toLower(firstName) == storedFirst &&
					toLower(lastName) == storedLast &&
					password == storedPass) {

					// Creation de l'objet User connecte
					User* loggedInUser = new User();
					loggedInUser->userID = user["userID"];
					loggedInUser->firstName = user["firstName"];
					loggedInUser->lastName = user["lastName"];
					loggedInUser->password = user["password"];
					loggedInUser->email = user["email"];
					loggedInUser->phone = user["phone"];

					cout << "\nConnexion reussie! Bienvenue " << firstName << " " << lastName << "!" << endl;
					return loggedInUser;
				}
			}
			catch (...) {
				continue; // Ignorer les entrees invalides
			}
		}

		attempts--;
		if (attempts > 0) {
			cout << "\nErreur: Informations incorrectes. Options:" << endl;
			cout << "1. Reessayer (" << attempts << " tentatives restantes)" << endl;
			cout << "2. Creer un compte" << endl;
			cout << "3. Quitter" << endl;
			cout << "Choix (1-3): ";

			string choice;
			getline(cin, choice);

			if (choice == "2") {
				return createNewUser();
			}
			else if (choice == "3") {
				return nullptr;  // Retourne nullptr pour indiquer la sortie
			}
		}
	}

	cout << "Trop de tentatives echouees." << endl;
	return nullptr;
}

/*
json* findCharitySearch(json& charitiesData) {
	cout << "\nRechercher par :\n";
	cout << "1. ID\n";
	cout << "2. Nom\n";
	cout << "Votre choix (1-2): ";

	int searchChoice;
	cin >> searchChoice;
	cin.ignore();

	if (searchChoice == 1) {
		int charityID;
		cout << "Entrez l'ID de la charite: ";
		cin >> charityID;
		cin.ignore();

		for (auto& charity : charitiesData["charities"]) {
			if (charity["charityID"] == charityID) {
				return &charity;
			}
		}
	}
	else if (searchChoice == 2) {
		string searchName;
		cout << "Entrez le nom (ou partie du nom): ";
		getline(cin, searchName);

		transform(searchName.begin(), searchName.end(), searchName.begin(), ::tolower);

		int matchCount = 0;
		json* lastMatch = nullptr;

		size_t index = 0;
		for (auto& charity : charitiesData["charities"]) {
			string name = charity["name"];
			transform(name.begin(), name.end(), name.begin(), ::tolower);

			if (name.find(searchName) != string::npos) {
				++matchCount;
				lastMatch = &charity;

				cout << matchCount << ". " << charity["name"]
					<< " (ID: " << charity["charityID"] << ")\n";
			}
		}

		if (matchCount == 0) {
			cout << "Aucune charite trouvee.\n";
			return nullptr;
		}
		else if (matchCount == 1) {
			return lastMatch;
		}
		else {
			cout << "Choisissez un numero (1-" << matchCount << "): ";
			int choice;
			cin >> choice;
			cin.ignore();

			int current = 0;
			for (auto& charity : charitiesData["charities"]) {
				string name = charity["name"];
				transform(name.begin(), name.end(), name.begin(), ::tolower);
				if (name.find(searchName) != string::npos) {
					current++;
					if (current == choice) {
						return &charity;
					}
				}
			}
		}
	}

	cout << "Charite non trouvee.\n";
	return nullptr;
}
// Fonction utilitaire pour trouver une charite
json* findCharity(json& data, const string& searchTerm) {
	for (auto& charity : data["charities"]) {
		if (to_string(charity["charityID"]) == searchTerm || charity["name"] == searchTerm) {
			return &charity;
		}
	}
	return nullptr;
}
*/
// Fonction pour sauvegarder dans JSON
void saveUserToJSON(User* user, const string& filename = "utilisateurs.json") {
	// 1. Verifier/generer l'ID unique
	if (user->userID >= 0) {
		user->userID = getNextID("utilisateurs.json", "utilisateurs", "userID");
		cout << "ID auto-genere : " << user->userID << endl;
	}
	ifstream inFile(filename);
	json data;

	unsigned long hashedPassword = hashString(user->password);
	// 2. Preparer les donnees
	//string hashedPassword = Gethash(user->password);
	json newUser = {
		{"userID", user->userID},
		{"firstName", user->firstName},
		{"lastName", user->lastName},
		{"password", hashedPassword},
		{"email", user->email},
		{"phone", user->phone}
	};

	// 3. Charger les donnees existante

	if (inFile.good()) {
		try {
			inFile >> data;
		}
		catch (const exception& e) {
			cerr << "Erreur lecture JSON: " << e.what() << endl;
			return;
		}
	}
	inFile.close();

	// 4. Initialiser si fichier vide/nouveau
	if (!data.contains("utilisateurs")) {
		data["utilisateurs"] = json::array();
	}

	// 5. Verifier les doublons
	for (const auto& u : data["utilisateurs"]) {
		if (u["email"] == user->email) {
			cerr << "Erreur: Email deja utilise" << endl;
			return;
		}
		if (u["userID"] == user->userID) {
			// Normalement impossible grâce a getUniqueID()
			cerr << "Erreur critique: ID deja existant" << endl;
			return;
		}
		if (u["phone"] == user->phone) {
			cerr << "Erreur: Numero de telephone deja enregistre" << endl;
			return;
		}
	}

	// 6. Ajouter et sauvegarder
	data["utilisateurs"].push_back(newUser);

	try {
		ofstream outFile(filename);
		if (!outFile.is_open()) {
			cerr << "Erreur creation fichier" << endl;
			return;
		}
		outFile << data.dump(4);
		cout << "Utilisateur " << user->firstName << " " << user->lastName
			<< " enregistre avec succès (ID: " << user->userID << ")!" << endl;
	}
	catch (const exception& e) {
		cerr << "Erreur ecriture: " << e.what() << endl;
	}
}
void saveCharityToJSON(const json& charitiesData, const string& filename = "charities.json") {
	/*// 1. Verifier/generer l'ID unique si necessaire
	if (ch->charityID < 0) {
		ch->charityID = getNextID("charities.json", "charities", "charityID");
		cout << "ID auto-genere : " << ch->charityID << endl;
	}

	// 2. Preparer les donnees de la charite
	json newCharity = {
		{"charityID", ch->charityID},
		{"name", ch->name},
		{"description", ch->description},
		{"targetAmount", ch->targetAmount},
		{"currentAmount", ch->currentAmount},
		{"deadline", {
			{"day", ch->d.tm_mday},
			{"month", ch->d.tm_mon + 1}, // tm_mon est 0-11
			{"year", ch->d.tm_year + 1900}, // tm_year est depuis 1900
			{"hour", ch->d.tm_hour},
			{"minute", ch->d.tm_min}
		}},
		{"status", ch->status}
	};

	// 3. Charger les donnees existantes
	json data;
	ifstream inFile(filename);

	if (inFile.good()) {
		try {
			inFile >> data;
		}
		catch (...) {
			data = json::object();
		}
	}
	inFile.close();

	// 4. Initialiser si fichier vide/nouveau
	if (!data.contains("charities")) {
		data["charities"] = json::array();
	}

	// 5. Verifier les doublons
	for (const auto& c : data["charities"]) {
		if (c["charityID"] == ch->charityID) {
			cerr << "Erreur: ID de charite deja existant" << endl;
			return;
		}
		if (c["name"] == ch->name) {
			cerr << "Erreur: Nom de charite deja utilise" << endl;
			return;
		}
	}

	// 6. Ajouter et sauvegarder
	data["charities"].push_back(newCharity);

	try {
		ofstream outFile(filename);
		if (!outFile.is_open()) {
			cerr << "Erreur creation fichier" << endl;
			return;
		}
		outFile << data.dump(4);
		cout << "Charite \"" << ch->name
			<< "\" enregistree avec succès (ID: " << ch->charityID << ")!" << endl;
	}
	catch (const exception& e) {
		cerr << "Erreur ecriture: " << e.what() << endl;
	}*/
	try {
		ofstream outFile(filename);
		if (!outFile.is_open()) {
			throw runtime_error("Impossible d'ouvrir le fichier pour ecriture");
		}

		// Formatage avec indentation pour une meilleure lisibilite
		outFile << charitiesData.dump(4);
		outFile.close();

		cout << " Donnees sauvegardees dans " << filename << endl;
	}
	catch (const exception& e) {
		cerr << "ERREUR lors de la sauvegarde: " << e.what() << endl;
	}
}
void saveDonationToJSON(const donation& don, int userID,const string& filename = "donations.json") {
	if (userID <= 0) {
		cerr << "Erreur : ID utilisateur invalide (" << userID << ")" << endl;
		return;
	}

	json donationsData = loadDonationsData(filename);

	// Obtenir la date et l'heure actuelles
	string donationDate = getCurrentDate();
	string donationTime = getCurrentTime();
	
	// Preparer la nouvelle donation
	json newDonation = {
		{"donationID", don.donationID},
		{"userID", userID},
		{"charityID", don.charityID},
		{"amount", don.amount},
		{"date", donationDate},  // Format: DD/MM/YYYY
		{"time", donationTime},  // Format: HH:MM
	};

	// Ajouter a la liste des donations
	donationsData["donations"].push_back(newDonation);

	// Sauvegarder dans le fichier
	try {
		ofstream outFile(filename);
		if (!outFile.is_open()) {
			cerr << "Erreur: Impossible d'ouvrir le fichier " << filename << " pour sauvegarde." << endl;
			return;
		}
		outFile << donationsData.dump(4); // Indentation de 4 espaces pour une meilleure lisibilite
		cout << " Don enregistre avec succès (ID: " << don.donationID << ")" << endl;
	}
	catch (const exception& e) {
		cerr << "Erreur lors de la sauvegarde: " << e.what() << endl;
	}

	// Mettre a jour le montant collecte dans la charite (modifiier le current amount dans la charite choisis)
	json charitiesData;
	ifstream inFile("charities.json");
	if (inFile.good()) {
		try {
			inFile >> charitiesData;
			inFile.close();

			for (auto& charity : charitiesData["charities"]) {
				if (charity["charityID"] == don.charityID) {
					double currentAmount = charity["currentAmount"].get<double>();
					charity["currentAmount"] = currentAmount + don.amount;
					break;
				}
			}

			ofstream charityOut("charities.json");
			charityOut << charitiesData.dump(4);
			charityOut.close();
		}
		catch (...) {
			cerr << "Erreur lors de la mise a jour du montant collecte." << endl;
		}
	}
}


void deleteCharity(json& charitiesData) {
	
	displayTousCharities(charitiesData);

	string userInput;
	cout << "\nEntrez l'ID ou le nom de la charite a supprimer: ";
	getline(cin, userInput);

	bool found = false;
	json updatedCharities = json::array();

	for (auto& charity : charitiesData["charities"]) {
		// Verification par ID (converti en string) ou par nom
		bool isMatch = (to_string(charity["charityID"]) == userInput) ||
			(charity["name"].get<string>() == userInput);

		if (isMatch) {
			found = true;
			cout << "\nVous êtes sur le point de supprimer: " << charity["name"]
				<< " (ID: " << charity["charityID"] << ")\n"
				<< "Confirmer la suppression? (O/N): ";

			char confirm;
			cin >> confirm;
			cin.ignore();

			if (tolower(confirm) == 'o') {
				cout << "Charite supprimee avec succès." << endl;
				// Ne pas ajouter a updatedCharities = suppression
			}
			else {
				updatedCharities.push_back(charity);
				cout << "Suppression annulee." << endl;
			}
		}
		else {
			updatedCharities.push_back(charity);
		}
	}

	if (found) {
		charitiesData["charities"] = updatedCharities;
	}
	else {
		cout << "Aucune charite trouvee avec cet ID ou ce nom." << endl;
	}
}
void addCharity(json& charitiesData) {
	// Initialiser la nouvelle charite
	charity newCharity;
	newCharity.currentAmount = 0.0; // Initialiser a 0 par defaut
	newCharity.status = "Active";   // Statut par defaut

	try {
		// Charger ou creer le fichier
		json charitiesData = loadOrCreateCharitiesFile();

		
		newCharity.charityID = getNextID("charities.json", "charities", "charityID");
		cout << "\n=== Creation d'une nouvelle charite (ID: " << newCharity.charityID << ") ===\n";

		// Saisie du nom avec validation
		bool validName = false;
		while (!validName) {
			cout << "Nom de la charite (min 3 caractères): ";
			getline(cin, newCharity.name);

			if (newCharity.name.length() < 3) {
				cout << "Le nom doit contenir au moins 3 caractères.\n";
				continue;
			}

			//verifier si unique
			bool nameExists = false;
			for (const auto& charity : charitiesData["charities"]) {
				if (charity["name"] == newCharity.name) {
					nameExists = true;
					break;
				}
			}

			if (nameExists) {
				cout << "Ce nom est deja utilise. Veuillez en choisir un autre.\n";
			}
			else {
				validName = true;
			}
		}
		//  Saisie de la description
		cout << "Description (appuyez sur Entree pour laisser vide): ";
		getline(cin, newCharity.description);
		if (newCharity.description.empty()) {
			newCharity.description = "Aucune description fournie";
		}

		// Saisie du montant cible avec validation
		bool validTarget = false;
		while (!validTarget) {
			cout << "Montant cible ($): ";
			string input;
			getline(cin, input);

			try {
				newCharity.targetAmount = stod(input);
				if (newCharity.targetAmount <= 0) {
					cout << "Le montant doit être positif.\n";
				}
				else {
					validTarget = true;
				}
			}
			catch (...) {
				cout << "Veuillez entrer un nombre valide (ex: 500.50).\n";
			}
		}

		//current amount
		bool validCurrent = false;
		while (!validCurrent) {
			cout << "Montant deja collecte [$] (Entree pour 0): ";
			string input;
			getline(cin, input);

			if (input.empty()) {
				newCharity.currentAmount = 0.0;
				validCurrent = true;
			}
			else {
				try {
					double amount = stod(input);
					if (amount < 0) {
						cout << "Le montant ne peut pas être negatif.\n";
					}
					else if (amount > newCharity.targetAmount) {
						cout << "Attention: Le montant depasse la cible ("
							<< newCharity.targetAmount << "$). Confirmer? (O/N): ";
						char confirm;
						cin >> confirm;
						cin.ignore();
						if (toupper(confirm) == 'O') {
							newCharity.currentAmount = amount;
							validCurrent = true;
						}
					}
					else {
						newCharity.currentAmount = amount;
						validCurrent = true;
					}
				}
				catch (...) {
					cout << "Veuillez entrer un nombre valide.\n";
				}
			}
		}
		// Date limite
		bool validDate = false;
		while (!validDate) {
			cout << "Date limite (JJ/MM/AAAA ou Entree pour non specifiee): ";
			string dateStr;
			getline(cin, dateStr);
			/* si allocation dinamique
			 tm* datetime = new tm;
			if (isValidDateTime(datetimeStr, datetime)) {
			char buffer[20];
			strftime(buffer, 20, "%d/%m/%Y,%H:%M", datetime);
			newCharity["deadline"] = buffer;
			break;
			}*/

			if (dateStr.empty()) {
				time_t now = time(nullptr);
				localtime_s(&newCharity.d, &now); // Date actuelle par defaut
				validDate = true;
			}
			else {
				if (isValidDateTime(dateStr, newCharity.d)) {
					validDate = true;
				}
				else {
					cout << "Format invalide. Utilisez JJ/MM/AAAA.\n";
				}
			}
		}

		// Ajout aux donnees
		json newCharityJson = {
			   {"charityID", newCharity.charityID},
			   {"name", newCharity.name},
			   {"description", newCharity.description},
			   {"targetAmount", newCharity.targetAmount},
			   {"currentAmount", newCharity.currentAmount},
			   {"deadline", tmToString(newCharity.d)},
			   {"status", newCharity.status}
		};

		charitiesData["charities"].push_back(newCharityJson);
		saveCharityToJSON(charitiesData);
		cout << "\nCHARITE AJOUTEE AVEC SUCCES!!\n";
		display1Charity(&newCharity);
	}
	catch (const exception& e) {
		cerr << "\n ERREUR: " << e.what() << endl;
		cerr << "la charite n'a pas pu etre ajoutee \n";
	}
}
void update_charity_field(json& data, int charityID, const std::string& field, const json& newValue) {
	for (auto& charity : data["charities"]) {
		if (charity["charityID"] == charityID) {
			if (charity.contains(field)) {
				charity[field] = newValue;
			}
			else {
				std::cerr << "Field \"" << field << "\" does not exist in charity object.\n";
			}
			break;
		}
	}
}

void modifyCharity(json & charitiesData) {
		string userInput;
		cout << "\nEntrez l'ID ou le nom de la charite a modifier: ";
		getline(cin ,userInput);
		int userID = std::stoi(userInput);
		// Recherche de la charite
		json* charityToModify = nullptr;// allocation dynamique
		for (auto& charity : charitiesData["charities"]) {
			if (to_string(charity["charityID"]) == userInput || charity["name"] == userInput) {
				charityToModify = new json(charity); // Allocation dynamique
				break;
			}
		}

		if (!charityToModify) {
			cout << "Aucune charite trouvee avec cet ID/ce nom.\n";
			return;
		}
		// Affichage des informations actuelles
		cout << "\nModification de la charite:\n";
		cout << "1. Nom: " << (*charityToModify)["name"] << endl;
		cout << "2. Description: " << (*charityToModify)["description"] << endl;
		cout << "3. Montant cible: " << (*charityToModify)["targetAmount"] << "$" << endl;
		cout << "4. Montant collecte: " << (*charityToModify)["amountRaised"] << "$" << endl;
		cout << "5. Date limite: " << (*charityToModify)["deadline"] << endl;
		cout << "6. Statut: " << (*charityToModify)["status"] << endl;


		bool modified = false;
		string choiceStr;

		while (true) {
			cout << "\nEntrez le numero du champ a modifier (1-6) ou 0 pour terminer: ";
			getline(cin, choiceStr);



			try {
				int choice = choiceStr.empty() ? -1 : stoi(choiceStr);

				if (choiceStr == "0") break;

				if (choice < 1 || choice > 6) {
					cout << "Option invalide!\n";
					continue;
				}

				switch (choice) {
				case 1: { // Nom
					while (true) {
						cout << "Nouveau nom (actuel: " << (*charityToModify)["name"] << "): ";
						string newName;
						getline(cin, newName);
						if (newName.length() >= 3) {
							// Verification unicite du nom
							bool nameExists = false;
							for (const auto& charity : charitiesData["charities"]) {
								if (charity["name"] == newName && charity["charityID"] != (*charityToModify)["charityID"]) {
									nameExists = true;
									break;
								}
							}

							if (!nameExists) {

								update_charity_field(charitiesData, userID, "name", newName);
								cout << "\nAfter name update:\n" << charitiesData.dump(4) << "\n";
								modified = true;
								break;
							}
							else {
								cout << "Ce nom est deja utilise par une autre charite.\n";
							}
						}
						else {
							cout << "Le nom doit contenir au moins 3 caractères.\n";
						}
					}
					break;
				}
				case 2: { // Description
					cout << "la nouvelle description: ";
					string newDesc;
					getline(cin, newDesc);
					(*charityToModify)["description"] = newDesc.empty() ? "Aucune description" : newDesc;
					update_charity_field(charitiesData, userID, "description", newDesc);
					break;
				}
				case 3: { // montant cible(target amount )
					while (true) {
						string amountStr;
						cout << "nouveau montant cible : ";
						getline(cin, amountStr);
						try {
							double amount = stod(amountStr);
							if (amount > 0) {
								update_charity_field(charitiesData, userID, "targetAmount", amount);
								break;
							}
							else {
								cout << "Le montant doit être positif.\n";
							}
						}
						catch (...) {
							cout << "Veuillez entrer un nombre valide.\n";
						}
					}
					break;
				}
				case 4: {//current amount
					cout << "nouveau montanty collecte : ";
					string amountStr;
					getline(cin, amountStr);

					// Option pour annuler
					if (amountStr.empty()) {
						cout << "Modification annulee.\n";
						break;
					}
					try {
						size_t pos;
						double amount = stod(amountStr, &pos);

						// Verifie que tout l'input a ete converti
						if (pos != amountStr.length()) {
							throw invalid_argument("Format invalide");
						}

						if (amount < 0) {
							cout << "Erreur: Le montant ne peut pas être negatif.\n";
						}
						else if (amount > (*charityToModify)["targetAmount"]) {
							cout << "Attention: Le montant collecte (" << amount
								<< " $) depasse le montant cible ("
								<< (*charityToModify)["targetAmount"] << " $).\n"
								<< "Confirmez-vous cette valeur? (O/N): ";

							char confirm;
							cin >> confirm;
							cin.ignore();
							if (tolower(confirm) == 'o') {
								update_charity_field(charitiesData, userID, "currentAmount", amount);
								modified = true;
								cout << "Montant collecte mis a jour.\n";
								break;
							}
						}
						else {
							update_charity_field(charitiesData, userID,"currentAmount" ,amount );
							modified = true;
							cout << "Montant collecte mis a jour.\n";
							break;
						}
					}
					catch (...) {
						cout << "Erreur: Veuillez entrer un nombre valide (ex: 450.25).\n";
					}
					break;

				}
				case 5: { //statut
					cout << "noueau statut: ";
					string newStatus;
					getline(cin, newStatus);
					if (!newStatus.empty()) {
						update_charity_field(charitiesData, userID, "status", newStatus);
					}
					break;
				}
				case 6: {
					// deadline
					while (true) {
						cout << "Nouvelle date limite : ";
						string newDate;
						getline(cin, newDate);

						if (newDate.empty() ||
							(newDate.length() == 10 && newDate[2] == '/' && newDate[5] == '/')) {
							(*charityToModify)["deadline"] = newDate.empty() ? "Non specifiee" : newDate;
							update_charity_field(charitiesData, userID, "deadline", newDate);
							break;
						}
						else {
							cout << "Format invalide. Utilisez JJ/MM/AAAA ou laissez vide.\n";
						}
					}
					break;
				}
				}
			}
			catch (...) {
				cout << "Veuillez entrer un nombre valide.\n";
			}
		}
		cout << "\n-------------------------------------\n";
		cout << "      RECAPITULATIF DE LA CHARITE\n";
		cout << "----------------------------------------\n";
		cout << "ID: " << (*charityToModify)["charityID"] << "\n";
		cout << "Nom: " << (*charityToModify)["name"] << "\n";
		cout << "Description: " << (*charityToModify)["description"] << "\n";
		cout << "Montant cible: " << (*charityToModify)["targetAmount"] << " $\n";
		cout << "Montant collecte: " << (*charityToModify)["currentAmount"] << " $\n";
		cout << "Date limite: " << (*charityToModify)["deadline"] << "\n";
		cout << "Statut: " << (*charityToModify)["status"] << "\n";
		cout << "--------------------------------------------\n";

		if (modified) {
			try {
				saveCharityToJSON(charitiesData);
				cout << " Modifications sauvegardees!\n";
			}
			catch (const exception& e) {
				cerr << "Erreur lors de la sauvegarde: " << e.what() << endl;
			}
		}
		else {
			cout << "Aucune modification effectuee.\n";
		}
		// Liberation de la memoire
		delete charityToModify;
}

void adminaccess(User* user) {

	if (user == nullptr) {
		cerr << "Erreur critique : Aucun utilisateur connecte!" << endl;
		return;
	}
	json charitiesData = loadOrCreateCharitiesFile();
	json donationsData = loadDonationsData();

	int choice;

	do {
		// Afficher d'abord la liste des charites

		cout << "\n=== MENU ADMINISTRATEUR ===" << endl;
		cout << "1. Afficher toutes les charites" << endl;
		cout << "2. Ajouter une charite" << endl;
		cout << "3. Supprimer une charite" << endl;
		cout << "4. Modifier une charite" << endl;
		cout << "5. Faire un don" << endl;
		cout << "6. Sauvegarder et quitter" << endl;
		cout << "Votre choix (1-6): ";
		cin >> choice;
		cin.ignore();
		try {

			switch (choice) {
			case 1: {
				// Afficher les charites disponibles
				displayTousCharities(charitiesData);
				break;
			}
			case 2: {
				cout << "\n===== Ajout d'une nouvelle charite =====" << endl;
				addCharity(charitiesData);
				charitiesData = loadOrCreateCharitiesFile();
				break;
			}
			case 3: {
				cout << "\n=== Suppression d'une charite ===" << endl;
				deleteCharity(charitiesData);
				saveCharityToJSON(charitiesData);
				break;
			}
			case 4: {
				modifyCharity(charitiesData);
				saveCharityToJSON(charitiesData);
				break;
			}
			case 5: {
				cout << "\n=== FAIRE UN DON ===" << endl;
				displayTousCharities(charitiesData);

				int charityID;
				double amount;
				bool validInput = false;


				do {
					cout << "Entrer l'ID de la charite : ";
					cin >> charityID;
					cin.ignore();

					bool charitiespres = false;
					for (const auto& charity : charitiesData["charities"]) {
						if (charity["charityID"] == charityID) {
							charitiespres = true;
						}
					}

					if (!charitiespres) {
						cout << "Erreur !!! charite introuvable!" << endl;
					}
					else {
						validInput = true;
					}
				} while (!validInput);

				bool valideamount = false;
				do {
					cout << "Montant du don (en $): ";
					cin >> amount;
					cin.ignore();

					if (amount <= 0) {
						cout << "Erreur: Le montant doit etre positif!" << endl;

					}
					else {
						valideamount = true;
					}


				} while (!valideamount);

				donation newDon;
				newDon.donationID = getNextID("donations.json", "donations", "donationID");
				newDon.charityID = charityID;
				newDon.amount = amount;
				
				saveDonationToJSON(newDon, user->userID );

				for (auto& charity : charitiesData["charities"]) {
					if (charity["charityID"] == charityID) {
						double current = charity["currentAmount"].get<double>();
						charity["currentAmount"] = current + amount;

						// Verifier si le montant cible est atteint
						if (charity["currentAmount"] >= charity["targetAmount"]) {
							charity["status"] = "Objectif atteint";
						}
						break;
					}
				}

				cout << "ID Donation: " << newDon.donationID << endl;
				cout << "Montant: " << fixed << setprecision(2) << newDon.amount << "$" << endl;
				cout << "Date: " << getCurrentDate() << endl;
				cout << "Heure: " << getCurrentTime() << endl;
				cout << "\n=====Don enregistre avec succes!=====" << endl;

				break;

			}
			case 6: {
				ofstream outFile("charities.json");
				if (outFile.is_open()) {
					outFile << setw(4) << charitiesData;
					cout << "\nDonnees sauvegardees avec succès.\n";
				}
				else {
					cerr << "Erreur lors de la sauvegarde!\n";
				}
				break;
			}
			default: {
				cout << "choix invalide.(entre 1--> 6) " << endl;
				break;
			}
			}
		}
		catch (const exception& e) {
			cerr << "Erreur: " << e.what() << endl;
		}
	} while (choice != 6);
}


void viewdonationsdo(int userID, const json& donationsData, const json& charitiesData) {
	if (!donationsData.contains("donations") || donationsData["donations"].empty()) {
		cout << "\nAucun don enregistre.\n";
		return;
	}

	bool found = false;
	double totalDonations = 0.0;

	cout << "\n=== HISTORIQUE DE VOS DONS ===\n";
	cout << "----------------------------------------------------\n";

	for (const auto& donation : donationsData["donations"]) {
		// Comparer les IDs en tant qu'entiers
		if (donation["userID"].get<int>() == userID) {
			found = true;
			totalDonations += donation["amount"].get<double>();

			// Rechercher le nom de la charite
			string charityName = "Inconnue";
			for (const auto& charity : charitiesData["charities"]) {
				if (charity["charityID"].get<int>() == donation["charityID"].get<int>()) {
					charityName = charity["name"];
					break;
				}
			}

			// Afficher les details du don
			cout << "ID Don: " << donation["donationID"] << endl;
			cout << "Charite: " << charityName << " (ID: " << donation["charityID"] << ")\n";
			cout << "Montant: " << fixed << setprecision(2) << donation["amount"].get<double>() << "$\n";
			cout << "Date: " << donation["date"] << " a " << donation["time"] << "\n";
			cout << "----------------------------------------------------\n";
		}
	}

	if (found) {
		cout << "TOTAL DONNe: " << fixed << setprecision(2) << totalDonations << "$\n";
	}
	else {
		cout << "Aucun don trouve pour votre compte.\n";
	}
}


void donateurmenu(User* user) {
	json charitiesData = loadOrCreateCharitiesFile();
	json donationsData = loadDonationsData();

	bool inDonationProcess = false;
	int currentCharityID = 0;
	double currentDonationAmount = 0.0;

	int choix;
	do {
		if (!inDonationProcess) {
			// Menu principal
			cout << "\n=== MENU DONATEUR ===" << endl;
			cout << "Bienvenue " << user->firstName << " " << user->lastName << endl;
			cout << "1. Voir la liste des charites" << endl;
			cout << "2. Faire un don" << endl;
			cout << "3. Voir mes dons" << endl;
			cout << "4. Quitter" << endl;
			cout << "Votre choix (1-4): ";
			cin >> choix;
			cin.ignore();

			switch (choix) {
			case 1: {
				displayTousCharities(charitiesData);
				break;
			}
			case 2: {
				inDonationProcess = true;
				displayTousCharities(charitiesData);
				bool validechar = false;

				do {
					cout << "Entrez l'ID de la charite (ou 0 pour annuler): ";
					cin >> currentCharityID;
					cin.ignore();

					if (currentCharityID <= 0) {
						inDonationProcess = false;
						break;
					}

					validechar = false;
					for (const auto& charity : charitiesData["charities"]) {
						if (charity["charityID"] == currentCharityID) {
							validechar = true;
							cout << "Charite selectionnee: " << charity["name"] << endl;
							break;
						}
					}

					if (!validechar) {
						cout << "ERREUR!!! Aucune charite trouvee avec cet id\n";
						continue;
					}

					bool validAmount = false;
					do {
						cout << "Entrez le montant du don (en $): ";
						cin >> currentDonationAmount;
						cin.ignore();

						if (currentDonationAmount <= 0) {
							cout << "Erreur: Le montant doit être positif. Veuillez reessayer.\n";
						}
						else {
							validAmount = true;
						}
					} while (!validAmount);

				} while (!validechar);
				break;
			}
			case 3: {
				viewdonationsdo(user->userID, donationsData, charitiesData);
				break;
			}
			case 4: {
				cout << "Au Revoir!!!\n";
				return;
			}
			default: {
				cout << "Choix invalide. Veuillez entrer un numero entre 1 et 4.\n";
				break;
			}
			}
		}
		else {
			// Menu pendant le processus de don
			cout << "\n=== PROCESSUS DE DON ===" << endl;
			cout << "Charite selectionnee: " << currentCharityID << endl;
			cout << "Montant actuel: " << fixed << setprecision(2) << currentDonationAmount << "$" << endl;
			cout << "\n=== MENU DONATION ===" << endl;
			cout << "1. Confirmer le don" << endl;
			cout << "2. Modifier le montant" << endl;
			cout << "3. Changer de charite" << endl;
			cout << "4. Annuler et retourner au menu principal" << endl;
			cout << "Votre choix (1-4): ";
			cin >> choix;
			cin.ignore();

			switch (choix) {
			case 1: {
				bool charityExists = false;
				for (const auto& charity : charitiesData["charities"]) {
					if (charity["charityID"] == currentCharityID) {
						charityExists = true;
						break;
					}
				}

				if (!charityExists) {
					cout << "Erreur: La charite selectionnee n'existe pas.\n";
					inDonationProcess = false;
					break;
				}

				if (currentDonationAmount <= 0) {
					cout << "Erreur: Le montant doit être positif.\n";
					break;
				}

				// Creer et enregistrer le don
				donation newDonation;
				newDonation.donationID = getNextID("donations.json", "donations", "donationID");
				newDonation.charityID = currentCharityID;
				newDonation.amount = currentDonationAmount;
				newDonation.date = getCurrentDate();
				newDonation.time = getCurrentTime();

				saveDonationToJSON(newDonation, user->userID, "donations.json");

				double newAmount = 0.0;
				
				// Mettre a jour la charite
				for (auto& charity : charitiesData["charities"]) {
					if (charity["charityID"] == currentCharityID) {
						charity["currentAmount"] = charity["currentAmount"].get<double>() + currentDonationAmount;
						if (charity["currentAmount"] >= charity["targetAmount"]) {
							charity["status"] = "Objectif atteint";
						}
						break;
					}
				}
				update_charity_field(charitiesData, currentCharityID, "currentAmount",newAmount);

				saveCharityToJSON(charitiesData, "charities.json");

				// Confirmation
				cout << "\nDon effectue avec succès!\n";
				cout << "ID Don: " << newDonation.donationID << endl;
				cout << "Montant: " << fixed << setprecision(2) << newDonation.amount << "$" << endl;
				cout << "Date: " << newDonation.date << endl;
				cout << "Heure: " << newDonation.time << endl;

				inDonationProcess = false;
				currentCharityID = 0;
				currentDonationAmount = 0.0;
				break;
			}
			case 2: {
				do {
					cout << "Montant du don (en $): ";
					cin >> currentDonationAmount;
					cin.ignore();

					if (currentDonationAmount <= 0) {
						cout << "NOTE !! Le montant doit etre positive!!!" << endl;
					}
				} while (currentDonationAmount <= 0);
				break;
			}
			case 3: {
				displayTousCharities(charitiesData);
				cout << "Entrez le nouvel ID de charite: ";
				cin >> currentCharityID;
				cin.ignore();

				bool charityExists = false;
				for (const auto& charity : charitiesData["charities"]) {
					if (charity["charityID"] == currentCharityID) {
						charityExists = true;
						break;
					}
				}
				if (!charityExists) {
					cout << "Charite non trouvee. La charite n'a pas ete modifiee." << endl;
					currentCharityID = 0;
				}
				break;
			}
			case 4: {
				inDonationProcess = false;
				currentCharityID = 0;
				currentDonationAmount = 0.0;
				cout << "Operation annulee.\n";
				break;
			}
			default: {
				cout << "Choix invalide.\n";
				break;
			}
			}
		}
	} while (true);
}

int main() {
	cout << "Salut ,bienvenue dans le programme de dons !" << endl;
	cout << "=============================================" << endl;
	cout << endl;


	int userCount = 0;

	int choix;
	User* currentUser = nullptr;

	cout << "\n1. Je suis un nouvel utilisateur\n";
	cout << "2. J'ai deja un compte\n";
	cout << "3. Quitter le programme \n";
	cout << "Votre choix (1 ou 2 ou 3): ";
	cin >> choix;
	cin.ignore();
	switch (choix) {
	case 1: {
		currentUser = createNewUser();
		saveUserToJSON(currentUser, "utilisateurs.json");
		displayUser(currentUser);
		break;
	}
	case 2: {
		currentUser= loginUser();
		break;
	}
	case 3: {
		cout << "\nMerci d'avoir utilise notre programme. a bientôt !\n";
		return 0;
	}
	default: {
		cout << "Choix invalide. Veuillez selectionner 1, 2 ou 3.\n";
		break;
	}
	}
	if (!currentUser) {
		cout << "Connexion echouee ou annulee. Fermeture du programme.\n";
		return 0;
	}

	string type = UserType(currentUser);  

	if (type == "admin") {
		adminaccess(currentUser);
	}
	else if (type == "donateur") {
		donateurmenu(currentUser);
	}
	else {
		cout << "Le choix est invalide." << endl;
		cout << "Au revoir." << endl;
	}
}

/*
void exportDonationsToPDF(const string& jsonFile, const string& pdfFile) {
	// 1. Charger les dons depuis le fichier JSON
	ifstream inFile(jsonFile);
	if (!inFile.is_open()) {
		cerr << "Erreur: impossible d'ouvrir le fichier JSON." << endl;
		return;
	}

	json data;
	try {
		inFile >> data;
	} catch (...) {
		cerr << "Erreur de lecture JSON." << endl;
		return;
	}

	// 2. Utiliser une map pour trier automatiquement les dons par date (clé = date)
	multimap<string, json> sortedDonations; // trie par date croissante
	for (const auto& don : data["donations"]) {
		if (don.contains("donorName") && don.contains("amount") && don.contains("date")) {
			sortedDonations.insert({ don["date"], don });
		}
	}

	// 3. Initialiser le PDF
	HPDF_Doc pdf = HPDF_New(nullptr, nullptr);
	if (!pdf) {
		cerr << "Erreur: impossible de créer le document PDF." << endl;
		return;
	}

	HPDF_Page page = HPDF_AddPage(pdf);
	HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);

	HPDF_Font font = HPDF_GetFont(pdf, "Helvetica", nullptr);
	HPDF_Page_SetFontAndSize(page, font, 12);

	float posY = 800;
	HPDF_Page_BeginText(page);
	HPDF_Page_TextOut(page, 50, posY, "Liste des dons (tries par date croissante):");
	posY -= 25;

	// 4. Écrire les dons dans le PDF
	for (const auto& [date, don] : sortedDonations) {
		string line = "Date: " + date +
					  " | Donateur: " + don["donorName"].get<string>() +
					  " | Montant: " + to_string(don["amount"].get<double>()) + "$";

		HPDF_Page_TextOut(page, 50, posY, line.c_str());
		posY -= 20;

		// Sauter à une nouvelle page si nécessaire
		if (posY < 50) {
			HPDF_Page_EndText(page);
			page = HPDF_AddPage(pdf);
			HPDF_Page_SetFontAndSize(page, font, 12);
			HPDF_Page_BeginText(page);
			posY = 800;
		}
	}

	HPDF_Page_EndText(page);

	// 5. Sauvegarder le PDF
	HPDF_SaveToFile(pdf, pdfFile.c_str());
	HPDF_Free(pdf);

	cout << "PDF genere avec succes!! " << pdfFile << endl;
}
*/