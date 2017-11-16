
library("lattice", lib.loc="C:/Program Files/R/R-3.4.0/library")
library(readr)
library("corrgram", lib.loc="~/R/win-library/3.4")
library("scatterplot3d", lib.loc="~/R/win-library/3.4")

out <- as.data.frame(read_csv("C:/Program Files (x86)/StarCraft/bwapi-data/write/output.txt", col_names = FALSE))
# out_2 <- as.data.frame(read_csv("C:/Program Files (x86)/StarCraft/bwapi-data/write/output_from_online_games.txt", col_names = FALSE))

names(out)<- c("delta_gas","gamma_supply","alpha_army","alpha_econ","alpha_tech","Race","Winner","shortct","medct","lct","map","opponent_name", "build_order")
names(out_2)<- c("delta_gas","gamma_supply","alpha_army","alpha_econ","alpha_tech","Race","Winner","shortct","medct","lct","map","opponent_name", "build_order")

# out<-rbind(out, out_2)
# out<-out[!duplicated(out),]
# new<-out[which( !out$opponent_name %in% c("Jormungand Brood" ,  "Auriga Tribe"  ,     "Epsilon Squadron" ,  "Sargas Tribe"    ,   "Furinax Tribe"
#                                          , "Ara Tribe"  ,        "Akilae Tribe"   ,    "Garm Brood"    ,     "Antiga"      ,       "Surtur Brood"
#                                          , "Leviathan Brood"  ,  "Elite Guard"   ,     "Baelrog Brood"    ,  "Tiamat Brood"  ,     "Velari Tribe"
#                                          , "Mar Sara"      ,    "Shelak Tribe"   ,    "Cronus Wing"    ,    "Fenris Brood"   ,    "Kel-Morian Combine"
#                                          , "Grendel Brood"   ,   "Venatir Tribe" ,     "Atlas Wing"   ,      "Delta Squadron"  )),]
 # write.table(out,"C:/Program Files (x86)/StarCraft/bwapi-data/write/output.txt", sep= ",", quote = FALSE, row.names = FALSE, col.names = FALSE)
# out<-new
 
#(out$shortct>=321 | out$Winner==1) -> out$Winner
out$race_win <- factor( paste ( out$Winner, out$Race , sep= " " ))
out$race_opp <- factor( paste ( out$Race, out$opponent_name , sep= " " ))

#out<-subset(out,out$Winner==1)

# histogram( ~out$delta_gas | out$race_win, type="count") #
# histogram( ~out$gamma_supply | out$race_win, type="count") #
# histogram( ~out$alpha_vis | out$race_win, type="count") #0.6
# histogram( ~out$alpha_army | out$race_win, type="count") #0.525
# histogram( ~out$alpha_econ | out$race_win, type="count") #0.008-0.009
# histogram( ~out$alpha_tech | out$race_win, type="count") #0.002 almost exactly.
# 
# by(out, out$Race, function(x) summary(glm( Winner ~ delta_gas + gamma_supply + alpha_army * alpha_econ * alpha_tech - alpha_army - alpha_econ - alpha_tech - alpha_army:alpha_econ:alpha_tech, family = quasibinomial(link = "logit"), data=x)))
# #by(out, out$Race, function(x) summary(glm( Winner ~ delta_gas + gamma_supply + alpha_army + alpha_econ + alpha_tech, family = quasibinomial(link = "logit"), data=x)))
# 
# aggregate( out[, c(1:6,8:12)], list(out$race_win), FUN=mean)
# 
# ks.test( c(out$delta_gas[out$Winner==1]), c(out$delta_gas[out$Winner==0]) )
# ks.test( c(out$gamma_supply[out$Winner==1]), c(out$gamma_supply[out$Winner==0]) )
# ks.test( c(out$alpha_army[out$Winner==1]), c(out$alpha_army[out$Winner==0]) )
# ks.test( c(out$alpha_econ[out$Winner==1]), c(out$alpha_econ[out$Winner==0]) )
# ks.test( c(out$alpha_tech[out$Winner==1]), c(out$alpha_tech[out$Winner==0]) )
# 
 histogram( ~ out$Winner | out$`Race`, xlab="Win % of Bot vs each Race")
 histogram( ~ out$Winner | out$`opponent_name`, xlab="Win % of Bot vs each NAME")
 histogram( ~ out$Winner | out$`race_opp`, xlab="Win % of Bot vs each Race")
 histogram( ~ out$Winner | out$`build_order`, xlab="Win % of Bot vs each Opening")
 
 table(out$opponent_name,out$Winner)
 
# summary(out)
 mav <- function(x,n=5){filter(x,rep(1/n,n), sides=2)}
 plot(mav(out$Winner[out$Race == "Zerg"] , 3))
 plot(mav(out$Winner[out$Race == "Terran"] , 3))
 plot(mav(out$Winner[out$Race == "Protoss"] , 3))
 
 t_z= 1:nrow(out[out$Race == "Zerg",]); t2 = t_z^2; t3 = t_z^3;
 reg_z<-glm(out$Winner[out$Race == "Zerg"] ~ t_z + t2 + t3, family = binomial(link = "logit") )
# summary(reg_z)
t_t= 1:nrow(out[out$Race == "Terran",]); t2 = t_t^2; t3 = t_t^3;
reg_t<-glm(out$Winner[out$Race == "Terran"] ~ t_t  + t2 + t3 , family = binomial(link = "logit") )

 t_p= 1:nrow(out[out$Race == "Protoss",]); t2 = t_p^2;  t3 = t_p^3;
 reg_p<-glm(out$Winner[out$Race == "Protoss"] ~ t_p  + t2 + t3 , family = binomial(link = "logit") )

plot(t_t,out$Winner[out$Race == "Terran"] ); #protoss should be longest, hopefully.
 points(t_z, reg_z$fitted.values, col = "#999999", type="l" )
 points(t_z, out$Winner[out$Race == "Zerg"], col = "#999999") #grey

 points(t_t, reg_t$fitted.values, col = "#E69F00", type="l" )
 points(t_t, out$Winner[out$Race == "Terran"], col =  "#E69F00") #orange
 
 points(t_p, reg_p$fitted.values, col = "#56B4E9", type="l" )
 points(t_p, out$Winner[out$Race == "Protoss"], col =  "#56B4E9") #blue
 

colors <- c("#999999", "#E69F00", "#56B4E9") 
unique(as.factor(out$Race[out$Winner==1])) #grey=p orange=t blue=z
colors <- colors[as.factor(out$Race[out$Winner==1])]
for(i in seq(0,360,30)){
  scatterplot3d(out$alpha_army[out$Winner==1],
            out$alpha_tech[out$Winner==1],
            out$alpha_econ[out$Winner==1],
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
} #[out$Winner==1] , [out$Winner==0], or none

corrgram(out[out$Winner==1,c(1:3, 5:6)], lower.panel = panel.pts, upper.panel = panel.conf, diag.panel = panel.density)
plot(out$alpha_tech[out$Winner==1], out$alpha_econ[out$Winner==1])
econ<-c(1,1000)
army<-c(1,1000)
tech<-c(1,1000)
for(i in 1:1000){
  a<-runif(1)*0.50+0.50
  b<-runif(1)*0.5
  c<-runif(1)*0.75+0.25
  econ[i]<-a/(a+b+c)
  tech[i]<-b/(a+b+c)
  army[i]<-c/(a+b+c)}
hist(econ)
hist(army)
hist(tech)