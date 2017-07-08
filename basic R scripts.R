library("lattice", lib.loc="C:/Program Files/R/R-3.4.0/library")
library(readr)

output_csv <- read_csv("C:/Program Files (x86)/StarCraft/Brood War/output.csv", col_names = FALSE)

names(output_csv)<- c("delta_gas","gamma_supply","alpha_army","alpha_vis","alpha_econ","alpha_tech","Race","Winner","shortct","medct","lct","seed")
                     

 (output_csv$shortct>=321 | output_csv$Winner==1) -> output_csv$Winner
output_csv$race_win <- factor( paste ( output_csv$Winner, output_csv$Race , sep= " " ))

histogram( ~output_csv$delta_gas | output_csv$race_win, type="count") #
histogram( ~output_csv$gamma_supply | output_csv$race_win, type="count") #
histogram( ~output_csv$alpha_vis | output_csv$race_win, type="count") #0.6
histogram( ~output_csv$alpha_army | output_csv$race_win, type="count") #0.525
histogram( ~output_csv$alpha_econ | output_csv$race_win, type="count") #0.008-0.009
histogram( ~output_csv$alpha_tech | output_csv$race_win, type="count") #0.002 almost exactly.

by(output_csv, output_csv$Race, function(x) summary(glm( Winner ~ delta_gas + gamma_supply + alpha_vis * alpha_army + alpha_econ + alpha_tech, family = quasibinomial(link = "logit"), data=x)))

aggregate( output_csv[, c(1:6,8:12)], list(output_csv$race_win), FUN=mean)


histogram( ~ output_csv$Winner | output_csv$`Race`, xlab="Win % of Bot vs each Race")
