
library("lattice", lib.loc="C:/Program Files/R/R-3.4.0/library")
library(readr)
library("corrgram", lib.loc="~/R/win-library/3.4")
library("scatterplot3d", lib.loc="~/R/win-library/3.4")

output_csv <- read_csv("C:/Program Files (x86)/StarCraft/Brood War/output.csv", col_names = FALSE)

names(output_csv)<- c("delta_gas","gamma_supply","alpha_army","alpha_vis","alpha_econ","alpha_tech","Race","Winner","shortct","medct","lct","seed")
                     

#(output_csv$shortct>=321 | output_csv$Winner==1) -> output_csv$Winner
output_csv$race_win <- factor( paste ( output_csv$Winner, output_csv$Race , sep= " " ))
#output_csv<-subset(output_csv,output_csv$Winner==1)

# histogram( ~output_csv$delta_gas | output_csv$race_win, type="count") #
# histogram( ~output_csv$gamma_supply | output_csv$race_win, type="count") #
# histogram( ~output_csv$alpha_vis | output_csv$race_win, type="count") #0.6
# histogram( ~output_csv$alpha_army | output_csv$race_win, type="count") #0.525
# histogram( ~output_csv$alpha_econ | output_csv$race_win, type="count") #0.008-0.009
# histogram( ~output_csv$alpha_tech | output_csv$race_win, type="count") #0.002 almost exactly.
# 
# by(output_csv, output_csv$Race, function(x) summary(glm( Winner ~ delta_gas + gamma_supply + alpha_army * alpha_econ * alpha_tech - alpha_army - alpha_econ - alpha_tech - alpha_army:alpha_econ:alpha_tech, family = quasibinomial(link = "logit"), data=x)))
# #by(output_csv, output_csv$Race, function(x) summary(glm( Winner ~ delta_gas + gamma_supply + alpha_army + alpha_econ + alpha_tech, family = quasibinomial(link = "logit"), data=x)))
# 
# aggregate( output_csv[, c(1:6,8:12)], list(output_csv$race_win), FUN=mean)
# 
# ks.test( c(output_csv$delta_gas[output_csv$Winner==1]), c(output_csv$delta_gas[output_csv$Winner==0]) )
# ks.test( c(output_csv$gamma_supply[output_csv$Winner==1]), c(output_csv$gamma_supply[output_csv$Winner==0]) )
# ks.test( c(output_csv$alpha_army[output_csv$Winner==1]), c(output_csv$alpha_army[output_csv$Winner==0]) )
# ks.test( c(output_csv$alpha_econ[output_csv$Winner==1]), c(output_csv$alpha_econ[output_csv$Winner==0]) )
# ks.test( c(output_csv$alpha_tech[output_csv$Winner==1]), c(output_csv$alpha_tech[output_csv$Winner==0]) )
# 
 histogram( ~ output_csv$Winner | output_csv$`Race`, xlab="Win % of Bot vs each Race")
# summary(output_csv)

 t_z= 1:nrow(output_csv[output_csv$Race == "Zerg",]); t2 = t_z^2; t3 = t_z^3;
 reg_z<-glm(output_csv$Winner[output_csv$Race == "Zerg"] ~ t_z + t2 + t3, family = binomial(link = "logit") )
# summary(reg_z)
t_t= 1:nrow(output_csv[output_csv$Race == "Terran",]); t2 = t_t^2; t3 = t_t^3;
reg_t<-glm(output_csv$Winner[output_csv$Race == "Terran"] ~ t_t + t2 + t3 , family = binomial(link = "logit") )

 t_p= 1:nrow(output_csv[output_csv$Race == "Protoss",]); t2 = t_p^2;  t3 = t_p^3;
 reg_p<-glm(output_csv$Winner[output_csv$Race == "Protoss"] ~ t_p + t2 + t3 , family = binomial(link = "logit") )

plot(t_t,output_csv$Winner[output_csv$Race == "Terran"] ); #protoss should be longest, hopefully.
 points(t_z, reg_z$fitted.values, col = "#999999", type="l" )
 points(t_z, output_csv$Winner[output_csv$Race == "Zerg"], col = "#999999")

 points(t_t, reg_t$fitted.values, col = "#E69F00", type="l" )
 points(t_t, output_csv$Winner[output_csv$Race == "Terran"], col =  "#E69F00")
 
 points(t_p, reg_p$fitted.values, col = "#56B4E9", type="l" )
 points(t_p, output_csv$Winner[output_csv$Race == "Protoss"], col =  "#56B4E9")
 

colors <- c("#999999", "#E69F00", "#56B4E9")
colors <- colors[as.factor(output_csv$Race[output_csv$Winner==1])]
for(i in seq(0,360,10)){
  scatterplot3d(output_csv$alpha_army[output_csv$Winner==1],
            output_csv$alpha_tech[output_csv$Winner==1],
            output_csv$alpha_econ[output_csv$Winner==1],
            xlab = "Alpha Army",
            ylab = "Alpha Tech",
            zlab = "Alpha Econ",
            angle = i,
            scale.y = 1,
            pch = 16, type="h",
            color = colors,
            #axis.scales = TRUE,
            xlim = c(0,1), ylim = c(0,1), zlim = c(0,1)
            )
} #[output_csv$Winner==1] , [output_csv$Winner==0], or none

corrgram(output_csv[output_csv$Winner==1,c(1:3, 5:6)], lower.panel = panel.pts, upper.panel = panel.conf, diag.panel = panel.density)
