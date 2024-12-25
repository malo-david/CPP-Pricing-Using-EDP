#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>

// Ce programme permet de calculer le prix d'une option européenne (pas d'exercice prématuré comme avec une option américaine ou bermudéenne) via la méthode des différences finies explicites de Black-Scholes.
// La méthode des différences finies est utilisée pour évaluer le prix de la call option, tandis que pour le put on utilise la parité put-call.
// Nous faisons les hypothèses suivantes:
// - Pas de dividende
// - Le prix de l'actif ne dépasse pas S_max (qui permet de discrétisé l'intervalle).
// Le programme permet non seulement à l'utilisateur de renseigner les caractéristiques de l'option, mais aussi de choisir la précision de l'analyse (pas spatial et temporel).
// Les Grecques (Delta et Gamma) sont calculés à la fin afin de donner des indications concernant les sensibilités du prix de l'option.

class FiniteDifferencePricer {
public:
    struct Parameters {
        double type;  // 1 pour call, 0 pour put
        double S0;    // Prix de l'actif sous-jacent initial
        double K;     // Prix Strike
        double r;     // Taux sans risque (en %)
        double sigma; // Volatilité de l'actif sous-jacent (en %)
        double T;     // Maturité de l'actif (en années)
    };

    FiniteDifferencePricer(const Parameters& params) // Initialise par défaut les paramètres de calculs des différences finies en utilisant les valeurs initiales de params
        : params(params), Smax(4.0 * params.K), N(50), M(2000) {}

    void run() {
        inputParameters(); // Permet à l'utilisateur de renseigner les caractéristiques de l'option
        int mode = chooseMode(); // Permet à l'utilisateur de choisir le mode de calcul (précision) qu'il souhaite
        configureMode(mode);
        if (!checkStability()) { // Au cas où la condition de stabilité n'est pas vérifiée, ce qui ne devrait pas arriver puisque le mode 3 ajuste automatiquement le pas temporel
            std::cerr << "Erreur : condition de stabilité non respectée.";
            return;
        }
        computeCallOptionPrice(); // Calcul du prix de l'option call
        displayResults(); // Affichage du prix de l'option, de Delta et de Gamma
    }

private:
    Parameters params; 
    double Smax;
    int N;  // Nombre de pas spatiaux 
    int M;  // Nombre de pas temporels
    double dS; // Pas spatial
    double dt; // Pas temporel
    std::vector<double> U; // U est le vecteur prix

    static inline double call_payoff(double S, double K) { // Définit la plus-value de l'option à maturité
        return std::max(S - K, 0.0);
    }

    void inputParameters() {
        std::cout << "Entrez les paramètres de l'option :\n";
        do {
        std::cout << "Type d'option (0 pour put, 1 pour call) : ";
        std::cin >> params.type;
        if (params.type != 0 && params.type != 1) {
            std::cerr << "Erreur : Entrez 0 ou 1.\n";
        }} while (params.type != 0 && params.type != 1);

        do {
        std::cout << "Prix de l'actif sous-jacent initial (S0) : ";
        std::cin >> params.S0;
        if (params.S0 <= 0) {
            std::cerr << "Erreur : S0 doit être strictement positif.\n";
        }} while (params.S0 <= 0);

        do {
        std::cout << "Prix Strike (K) : ";
        std::cin >> params.K;
        if (params.K <= 0) {
            std::cerr << "Erreur : K doit être strictement positif.\n";
        }} while (params.K <= 0);

        do {
            std::cout << "Taux sans risque (r) : ";
            std::cin >> params.r;
            if (params.r < 0 || params.r > 1) {
                std::cerr << "Erreur : r doit être entre 0 et 1. Exemple : 5% = 0.05.\n";
            }} while (params.r < 0 || params.r > 1);
        
        do {
            std::cout << "Volatilité (sigma) : ";
            std::cin >> params.sigma;
            if (params.sigma <= 0 || params.sigma > 1) {
                std::cerr << "Erreur : sigma doit être strictement positif et inférieur à 1.\n";
            }} while (params.sigma <= 0 || params.sigma > 1);
        
        do {
            std::cout << "Maturité (T) : ";
            std::cin >> params.T;
            if (params.T <= 0) {
                std::cerr << "Erreur : T doit être strictement positif.\n";
            }} while (params.T <= 0);
        Smax = 4.0 * params.K;
    }

    int chooseMode() {
        std::cout << "Choisissez un mode :\n";
        std::cout << "1. Précis (petits pas, mais exécution lente)\n";
        std::cout << "2. Rapide (grands pas, mais moins précis)\n";
        std::cout << "3. Personnalisé (nombre de pas spatial à choisir) \n";
        int mode;
        std::cin >> mode;
        return mode;
    }

    void configureMode(int mode) {
        switch (mode) {
        case 1: // Précis
            N = 2000;
            break;
        case 2: // Rapide
            N = 100;
            break;
        case 3: // Personnalisé
            std::cout << "Entrez le nombre de pas spatiaux (N) : ";
            std::cin >> N;
            break;
        default:
            std::cerr << "Mode invalide, utilisation du mode Rapide par défaut.\n";
            N = 100;
            break;;
        }

        // Définir des bornes dynamiques pour dt et M
        // Nécessaire dans des cas extrêmes où la volatilité est très faible et où la saturation de la condition de stabilité mène à des valeurs très petites pour M.
        const int M_target = 100;  // Minimum pour le nombre de pas temporels
        double dt_max = params.T / M_target;
        
        dS = Smax / N; 
        dt = std::min((dS * dS) / (params.sigma * params.sigma * Smax * Smax),dt_max); // Sature la condition de stabilité
        M = static_cast<int>(params.T / dt) + 1;
        std::cout << "Pas temporel calculé (dt) : " << dt << ", Nombre de pas temporels (M) : " << M << "\n";
    }

    bool checkStability() const { // Vérifie si la condition de stabilité du modèle est bien respectée (devrait toujours l'être car dans les 3 modes le pas de temps est choisi afin de respecter cette contrainte)
        return dt <= dS * dS / (params.sigma * params.sigma * Smax * Smax);
    }

    void computeCallOptionPrice() {
        U.assign(N + 1, 0.0); // Initialisation du vecteur prix 
        std::vector<double> U_old(N + 1, 0.0); // Création d'un deuxième vecteur qui gardera en mémoire les prix à la temporalité t+dt
        // Nous faisons le choix d'opter pour deux vecteurs. Nous aurions pu créer une matrice qui garderait l'historique des prix.
        // Ce choix rend l'implémentation plus légère, mais ne permet pas de conserver l'historique des prix complets.
        for (int j = 0; j <= N; j++) {
            U[j] = call_payoff(j * dS, params.K); // Conditions limites (à maturité) : le prix de l'option est donc la plus value
        }

        for (int m = M; m > 0; m--) {
            U_old = U;  // Copie du U actuel (qui deviendra l'ancien à l'étape d'après)
            for (int j = 1; j < N; j++) { // On définit les variables a, b et c qui interviennent dans la formule de récurrence
                double S = j * dS;
                double alpha = (params.sigma * params.sigma * S * S * dt) / (2.0 * dS * dS);
                double beta = (params.r * S * dt) / (2.0 * dS);
                double a = alpha - beta;
                double b = 1.0 - params.r * dt - 2.0 * alpha;
                double c = alpha + beta;
                U[j] = a * U_old[j - 1] + b * U_old[j] + c * U_old[j + 1];  // Calcul du nouveau vecteur prix U par la formule de récurrence
            }

            // Conditions aux limites (t=0)
            U[0] = 0.0; // j=0 : S=0, pour un call : U=0

            // t=0, j=N : S=Smax, condition limite : U(Smax,t)
            double tau = params.T - (m - 1) * dt; // Temps futur par rapport à la finalité
            U[N] = Smax - params.K * std::exp(-params.r * tau);
        }
    }

    // À présent, U correspond à la grille au temps t=0.
    // On récupère le prix pour S0. On doit interpoler si S0 n'est pas un point de grille exact.

    void displayResults() {
        double S0_index = params.S0 / dS;
        int j0 = static_cast<int>(std::floor(S0_index));
        double w = S0_index - j0; 
        
        double price_call = (j0 >= 0 && j0 < N) ? (1.0 - w) * U[j0] + w * U[j0 + 1] : U[j0]; // Il s'agit du prix du call

        // Calcul des Grecques (Delta et Gamma) pour le call par différences finies.
        // On utilise les points j0-1, j0, j0+1, en vérifiant que j0>0 et j0<N :

        double Delta_call = (j0 > 0 && j0 < N) ? (U[j0 + 1] - U[j0 - 1]) / (2.0 * dS) : 0.0;
        double Gamma = (j0 > 0 && j0 < N) ? (U[j0 + 1] - 2.0 * U[j0] + U[j0 - 1]) / (dS * dS) : 0.0; // Gamma est le même pour call et put
      
        double price = price_call;

        double Delta = Delta_call;
        
        if (params.type==0) {
        price = price_call - params.S0 + params.K * std::exp(-params.r * params.T); // parité put-call
        Delta = Delta_call - 1;
        }


        std::cout << "Prix de l'option : " << price << "\n";
        std::cout << "Delta : " << Delta << "\n";
        std::cout << "Gamma : " << Gamma << "\n";
    }
};

int main() {
    FiniteDifferencePricer::Parameters params{100.0, 135.0, 0.05, 0.2, 1.0}; // Configuration par défaut
    FiniteDifferencePricer pricer(params);
    pricer.run();
    return 0;
}
